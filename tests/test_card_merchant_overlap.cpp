//
// tests/test_card_merchant_overlap.cpp
//
// card-fraud-realism-v2 step b: THE MERCHANT-OVERLAP INSTRUMENT
// (contract docs/card_fraud_v2_roadmap.md; gate 1 of
// docs/card_fraud_online_gnn.md).
//
// The online-GNN audit found the corpus's blocking defect statistically
// — every fraud row landed on a merchant with no legitimate card row,
// so merchant identity alone classifies the corpus. That was measured
// once, by hand, in SQL against a live PostgreSQL build. This is the
// same measurement as a SERVERLESS gate, so the defect has a number in
// the suite BEFORE the fix lands and a number after it (instrument
// first — the standing lesson).
//
// It reimplements the audit's own SQL:
//
//   per card-rail merchant: fraud_rows, legitimate_rows
//   fraud_only_merchants          = merchants with fraud > 0, legit == 0
//   fraud_rows_at_fraud_only      = fraud rows sitting on those
//
// WHAT THIS PINS TODAY: only that the measurement is WELL-DEFINED —
// the card rail carries both fraud and legitimate rows, every row
// resolves to a merchant endpoint, and the shares add up. The headline
// ratio is PRINTED, not bounded, because the current generator is
// expected to score ~1.0 on it (the defect) and the step b-2 selection
// round is what drives it to ~0. b-2 tightens the assertion; this file
// deliberately does not encode "the defect exists" as a requirement,
// so the fix never has to fight its own gate.
//
// THE AXIS: the endpoint POPULATION, not the endpoint identity. Two
// disjoint destination pools (fraud draws the legit-transfer biller
// hubs, legitimate card purchases draw the merchant catalogue) is a
// structural property of the generator, not a seed artifact, so a
// single small world measures it faithfully.
//
// WORLD SHAPE (join-cohort round): the leg now carries the PRODUCTION
// join cohort — 8 joiners at 300 people over 730 days from 1991, by the
// BEA sizing in synth/personas/join.hpp — instead of the joinerless
// harness world this gate was first written against
// (window_leg_support.hpp, WORLD SHAPE). NOTHING TO RE-CALIBRATE HERE:
// this file bounds no measured value. Its four assertions are
// well-definedness preconditions (both label populations present, every
// row resolved, shares consistent), satisfied by margins of three orders
// of magnitude — 124 fraud and 42,369 legitimate card rows over 254
// merchants on the pre-flip world — and the headline ratio is printed.
// The world shape is now PINNED below so the measurement can never again
// silently describe a population production does not generate.
//

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"

#include "window_leg_support.hpp"

#include <cstdio>
#include <map>
#include <string>

namespace pl = ::PhantomLedger;
namespace channels = pl::channels;

using pltest::LegOptions;
using pltest::LegResult;
using Txn = pltest::Txn;

namespace {

constexpr std::uint64_t kSeed = 1234567;
constexpr std::int32_t kPopulation = 300;

struct Counts {
  std::size_t fraud = 0;
  std::size_t legit = 0;
};

int g_failures = 0;

void check(bool cond, const std::string &what) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

// The card rail: the channel both legitimate card purchases and the
// unauthorized card / gift-card fraud rails ride. Overlap is only
// meaningful within ONE rail — comparing a card fraud row against a
// bill-pay legit row would prove nothing about merchant identity.
[[nodiscard]] bool isCardRail(const Txn &t) {
  return t.session.channel.value ==
         channels::tag(channels::Legit::cardPurchase).value;
}

} // namespace

int main() {
  const auto pools = pltest::buildPoolSet(kSeed);

  pltest::announceLeg("card-rail overlap leg (1991, 730d)");
  LegOptions opt;
  opt.seed = kSeed;
  opt.window.start = pl::time::makeTime(pl::time::CalendarDate{1991, 1, 1});
  opt.window.days = 730;
  opt.population = kPopulation;
  opt.withBaseRoutines = true;
  opt.withFamily = true;
  const auto leg = pltest::runLeg(pools, opt);
  pltest::printLeg("card-overlap", leg);

  // WORLD SHAPE, asserted before anything is measured: this gate reports
  // on the population production generates, which has a join cohort. A
  // zero here means the leg rebuilt the pre-H3-3c-ii joinerless world and
  // every number below describes a world that never ships.
  check(leg.joiners > 0,
        "the leg carries the production join cohort (joiners " +
            std::to_string(leg.joiners) + ")");

  // Per-merchant fraud/legit tallies over the card rail. entity::Key
  // has a defaulted operator<=>, so it keys a std::map directly.
  std::map<pl::entity::Key, Counts> byMerchant;
  std::size_t cardFraudRows = 0;
  std::size_t cardLegitRows = 0;

  for (const auto &t : leg.rows) {
    if (!isCardRail(t)) {
      continue;
    }
    auto &cell = byMerchant[t.target];
    if (t.fraud.flag == 1) {
      ++cell.fraud;
      ++cardFraudRows;
    } else {
      ++cell.legit;
      ++cardLegitRows;
    }
  }

  std::size_t fraudMerchants = 0;
  std::size_t fraudOnlyMerchants = 0;
  std::size_t fraudRowsAtFraudOnly = 0;
  for (const auto &[key, cell] : byMerchant) {
    (void)key;
    if (cell.fraud == 0) {
      continue;
    }
    ++fraudMerchants;
    if (cell.legit == 0) {
      ++fraudOnlyMerchants;
      fraudRowsAtFraudOnly += cell.fraud;
    }
  }

  // The measurement must be well-defined before its value means
  // anything: both populations present, and every card row resolved to
  // some endpoint.
  check(cardFraudRows > 0,
        "card rail carries fraud rows (" + std::to_string(cardFraudRows) + ")");
  check(cardLegitRows > 0, "card rail carries legitimate rows (" +
                               std::to_string(cardLegitRows) + ")");
  check(!byMerchant.empty(), "card rail resolves to merchant endpoints (" +
                                 std::to_string(byMerchant.size()) + ")");
  check(fraudRowsAtFraudOnly <= cardFraudRows,
        "fraud rows at fraud-only merchants cannot exceed all fraud rows");

  const double fraudOnlyRowShare =
      cardFraudRows == 0
          ? 0.0
          : static_cast<double>(fraudRowsAtFraudOnly) /
                static_cast<double>(cardFraudRows);
  const double fraudOnlyMerchantShare =
      fraudMerchants == 0
          ? 0.0
          : static_cast<double>(fraudOnlyMerchants) /
                static_cast<double>(fraudMerchants);

  // THE HEADLINE NUMBER. ~1.0 means merchant identity alone separates
  // the labels (the audit's finding); step b-2 drives it toward 0 by
  // routing the card rails through the merchant acceptance population.
  std::printf("  world shape: join cohort %llu of %d people\n",
              static_cast<unsigned long long>(leg.joiners), kPopulation);
  std::printf("  card-rail merchants %zu (fraud-touched %zu, fraud-only "
              "%zu)\n",
              byMerchant.size(), fraudMerchants, fraudOnlyMerchants);
  std::printf("  card rows: fraud %zu, legit %zu\n", cardFraudRows,
              cardLegitRows);
  std::printf("  FRAUD-ONLY MERCHANT SHARE   %.4f\n", fraudOnlyMerchantShare);
  std::printf("  FRAUD ROWS AT FRAUD-ONLY    %.4f  <- b-2 drives this to ~0\n",
              fraudOnlyRowShare);

  if (g_failures != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("test_card_merchant_overlap: measurement well-defined "
              "(fraud-only row share %.4f)\n",
              fraudOnlyRowShare);
  return 0;
}
