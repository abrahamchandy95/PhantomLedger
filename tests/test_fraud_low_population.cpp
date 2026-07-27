//
// tests/test_fraud_low_population.cpp
//
// victimization-v1: THE RING-FREE FRAUD GATE
// (docs/card_fraud_victimization.md F1).
//
// THE DEFECT THIS PINS. Fraud ring count is
// `round(lognormal(6.0, 0.4) x population / 10000)` with NO floor
// (synth/people/rings.hpp), so it is ZERO below roughly population 833.
// A single blanket guard at the top of Injector::inject —
// `if (rings_.topology->rings.empty()) return {};` — used to short-
// circuit the ENTIRE fraud stage there. A 500-person, 30-year corpus
// came out with zero fraud rows of any kind, and nothing in the suite
// noticed: every fraud gate ran either at population 10000 or on the
// gate harness's scaledFraudProfile (perTenKMean 200), both of which
// always carry rings. This gate is that missing coverage.
//
// WHY THE FRAUD MUST STILL EXIST. The unauthorized family — card
// compromise, the gift-card scam, ATO — has no dependence on ring
// topology. `buildCompromisePlans` draws victims across the WHOLE
// roster (excluding ring participants, keeping the two populations
// disjoint), the attacker is exogenous by construction (source is the
// victim's own account, destination a catalogue merchant, IP a random
// address), and the budget rides `targetTxnFraudP x realized candidate
// count`. Someone social-engineering a customer from another country
// does not require a fraud ring resident in the sampled population.
//
// WHAT IS ASSERTED, and the pair matters more than either half:
//
//   1. This population plans ZERO rings. That is the PRECONDITION —
//      if a future profile change gives it rings, this gate stops
//      testing what it claims to and says so.
//   2. Fraud rows exist anyway, and reach THE CARD VIEW.
//   3. Fraud is EXCLUSIVELY the exogenous family: zero `launder_ring`
//      rows. No rings means no ring laundering, and that separation is
//      the whole design — not an accident of budget arithmetic.
//
// The victim share is PRINTED against a real-world comparator rather
// than gated: at a ~4%/year victimization hazard, a 2-year window puts
// roughly 1 - 0.96^2 = 7.8% of people victimized at least once. The
// model's own share should land in that neighbourhood, but the exact
// value is a consequence of the prevalence target and belongs to the
// prevalence suite, not here.
//

#include "phantomledger/pipeline/simulate.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/fraud/types.hpp"
#include "phantomledger/transactions/clearing/balance_book.hpp"
#include "phantomledger/transfers/channels/credit_cards/lifecycle.hpp"

#include "gate_world.hpp"

#include <cstdint>
#include <cstdio>
#include <exception>
#include <map>
#include <set>
#include <string>

namespace pl = ::PhantomLedger;
namespace channels = pl::channels;

namespace {

constexpr std::uint64_t kSeed = 11;

// Chosen to be comfortably BELOW the ring threshold: expected ring count
// is round(6.0 x 400/10000) = round(0.24) = 0, and the lognormal spread
// (sigma 0.4) does not lift 0.24 to 0.5 at any plausible draw.
constexpr std::int32_t kPopulation = 400;
constexpr int kDays = 730;

// A 2-year window at a ~4%/year hazard: 1 - 0.96^2.
constexpr double kRealWorldTwoYearShare = 0.078;

int g_failures = 0;

void check(bool condition, const std::string &what) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

[[nodiscard]] pl::time::Window window() {
  pl::time::Window w;
  w.start = pl::time::makeTime({1991, 1, 1});
  w.days = kDays;
  return w;
}

// The DEFAULT fraud profile on purpose. The gate harness's
// scaledFraudProfile (perTenKMean 200) would manufacture rings at any
// population and hide exactly the defect under test.
[[nodiscard]] pl::pipeline::SimulationResult
runWorld(const pl::synth::pii::PoolSet &poolSet,
         const pl::synth::people::Fraud &fraudProfile) {
  const auto w = window();

  const pl::pipeline::stages::entities::EntitySynthesis entities{
      .population = kPopulation,
      .identity =
          pl::synth::pii::IdentityContext{
              .pools = &poolSet,
              .simStart = w.start,
              .localeMix = pl::synth::pii::LocaleMix::usOnly(),
          },
      .fraud = fraudProfile,
  };

  pl::clearing::BalanceRules balanceRules{};
  pl::transfers::credit_cards::LifecycleRules lifecycleRules{};

  auto rng = pl::random::Rng::fromSeed(kSeed);
  pl::pipeline::SimulationPipeline pipeline{rng, w, entities, kSeed};
  pipeline.transferStage()
      .legit()
      .window(w)
      .seed(kSeed)
      .openingBalanceRules(&balanceRules)
      .creditLifecycle(&lifecycleRules);
  pipeline.transferStage().fraud().profile(&fraudProfile);

  return pipeline.run();
}

[[nodiscard]] bool inCardView(const pl::transactions::Transaction &t) {
  return t.session.channel.value ==
             channels::tag(channels::Legit::cardPurchase).value ||
         t.session.channel.value ==
             channels::tag(channels::Legit::merchant).value;
}

} // namespace

int main() {
  try {
    const auto poolSet = pltest::buildPoolSet(kSeed);
    const pl::synth::people::Fraud fraudProfile{}; // DEFAULTS

    std::printf("low-population fraud gate: %d people, %d days, DEFAULT "
                "fraud profile\n",
                kPopulation, kDays);
    std::fflush(stdout);
    const auto result = runWorld(poolSet, fraudProfile);

    // 1. THE PRECONDITION: this world plans no rings.
    const auto ringCount = result.people.roster.topology.rings.size();
    std::printf("  rings planned: %zu (expected 0 — the whole point)\n",
                ringCount);
    check(ringCount == 0,
          "population " + std::to_string(kPopulation) +
              " must plan ZERO rings for this gate to mean anything; it "
              "planned " +
              std::to_string(ringCount) +
              ". Lower kPopulation or this gate no longer tests the "
              "ring-free path.");
    if (g_failures != 0) {
      return 1;
    }

    // 2. Fraud exists anyway, and reaches the card view.
    std::map<pl::fraud::FraudType, std::size_t> byType;
    std::set<pl::entity::Key> victimSources;
    std::size_t fraudRows = 0;
    std::size_t cardViewFraud = 0;
    std::size_t launderRing = 0;

    for (const auto &t : result.transfers.ledger.posted.txns) {
      if (t.fraud.flag == 0) {
        continue;
      }
      ++fraudRows;
      ++byType[t.fraud.type];
      victimSources.insert(t.source);
      if (inCardView(t)) {
        ++cardViewFraud;
      }
      if (t.fraud.type == pl::fraud::FraudType::launderRing) {
        ++launderRing;
      }
    }

    std::printf("  corpus %zu rows, fraud %zu, of which card view %zu\n",
                result.transfers.ledger.posted.txns.size(), fraudRows,
                cardViewFraud);
    for (const auto &[type, count] : byType) {
      std::printf("    %-16s %zu\n",
                  std::string{pl::fraud::fraudTypeName(type)}.c_str(), count);
    }

    check(fraudRows > 0,
          "a ring-free population must STILL carry fraud: the "
          "unauthorized family's attacker is exogenous and its budget "
          "rides the realized candidate count, not ring topology");
    check(cardViewFraud > 0,
          "ring-free fraud must reach the CARD VIEW (the unauthorized "
          "card rail and gift-card scam ride card_purchase), got " +
              std::to_string(cardViewFraud));

    // 3. ...and it is EXCLUSIVELY the exogenous family.
    check(launderRing == 0,
          "with zero rings there can be no ring laundering, yet " +
              std::to_string(launderRing) +
              " launder_ring row(s) appeared — the family separation "
              "broke");

    // The victim share, printed against the real-world comparator.
    const double victimShare = static_cast<double>(victimSources.size()) /
                               static_cast<double>(kPopulation);
    std::printf("  distinct compromised sources %zu = %.2f%% of the "
                "population over 2 years (real-world comparator ~%.1f%%)\n",
                victimSources.size(), 100.0 * victimShare,
                100.0 * kRealWorldTwoYearShare);

    if (g_failures != 0) {
      std::fprintf(stderr, "\n%d check(s) failed\n", g_failures);
      return 1;
    }
  } catch (const std::exception &e) {
    std::fprintf(stderr, "FAIL: exception: %s\n", e.what());
    return 2;
  }

  std::printf("test_fraud_low_population: a ring-free world still carries "
              "exogenous-attacker fraud, and only that\n");
  return 0;
}
