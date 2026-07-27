//
// tests/test_card_victim_baselines.cpp
//
// victimization-v4: THE VICTIM-SIDE BASELINES AND THE EXPOSURE
// INSTRUMENT — docs/card_fraud_victimization.md D3.
//
// TWO JOBS, deliberately in one place because they share an expensive
// world build:
//
//   THE GATE (anti-shortcut). A persona-only and an age-only classifier
//   must not solve the task. V2 tilts victim selection by card exposure
//   and V3 will add a persona/age scam hazard; both make victim-side
//   attributes correlate with the label, which is realistic and is also
//   exactly how this arc's original merchant-identity defect got in. The
//   recorded law is that the baseline which would catch an over-strong
//   tilt must exist when the tilt lands.
//
//   THE INSTRUMENT (calibration). The per-persona picture V2 and V3 are
//   judged against. It reports TWO DIFFERENT AXES, and conflating them
//   was a real error in this gate's first version:
//
//     PER-PERSON INCIDENCE  victims / people. THIS is the axis the
//                           research speaks on ("~1 in 10 persons per
//                           year", "exposure rises with cards held and
//                           volume"), and the axis exposure weighting
//                           is supposed to move.
//     PER-ROW RATE          fraud rows / card rows. This is what a GNN
//                           sees per transaction, so it is the axis the
//                           shortcut gate must use.
//
//   They differ by the activity denominator: rate ~ incidence / rows,
//   so a low-activity persona can show a HIGH per-row rate while having
//   LOW incidence. Reading one as if it were the other inverts the
//   apparent gradient.
//
// INSTRUMENT-COVERAGE GATE. The first version resolved source keys
// through the account registry alone, so every `cardPurchase` row —
// whose source is the CREDIT-CARD key, not an account key — went
// unresolved: a third of the card view silently dropped, and not at
// random (unauthorized fraud sources the victim's deposit account and
// so survived, while legitimate credit-card purchases did not). That
// inflated the measured base rate and biased every per-persona figure.
// Sources now resolve through the card registry as well, and the
// unresolved SHARE is itself gated — a measurement instrument has to
// prove its own coverage, or it quietly measures a subset.
//

#include "phantomledger/entities/holdings/cards.hpp"
#include "phantomledger/pipeline/simulate.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/personas/timeline.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/personas/names.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"
#include "phantomledger/transactions/clearing/balance_book.hpp"
#include "phantomledger/transfers/channels/credit_cards/lifecycle.hpp"

#include "gate_world.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace pl = ::PhantomLedger;
namespace channels = pl::channels;
namespace personas = pl::personas;
namespace timeline = pl::synth::personas::timeline;

namespace {

constexpr std::uint64_t kSeed = 4242;
constexpr std::int32_t kPopulation = 600;
constexpr int kDays = 1461; // four whole years, 1991-1994

// Same bound as the merchant-identity baseline.
constexpr double kMaxRecallAtHighPrecision = 0.25;
constexpr double kPrecisionFloor = 0.90;

// 10-year bands keep group counts usable at N=600.
constexpr int kAgeBandYears = 10;

// The instrument must see essentially the whole card view. External
// counterparty sources can legitimately reach it, so this is not zero —
// but a third of the view going missing is a defect, not a rounding.
constexpr double kMaxUnresolvedShare = 0.01;

int g_failures = 0;

void check(bool condition, const std::string &what) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

struct Counts {
  std::size_t fraud = 0;
  std::size_t legit = 0;

  [[nodiscard]] std::size_t total() const { return fraud + legit; }
  [[nodiscard]] double rate() const {
    return total() == 0 ? 0.0
                        : static_cast<double>(fraud) /
                              static_cast<double>(total());
  }
};

// Per-person-cohort view, grouped by persona at WINDOW START (a stable
// per-person label; the per-row tables below use persona AT THE ROW).
struct Cohort {
  std::size_t people = 0;
  std::size_t victims = 0;
  std::size_t cardRows = 0;

  [[nodiscard]] double incidence() const {
    return people == 0 ? 0.0
                       : static_cast<double>(victims) /
                             static_cast<double>(people);
  }
  [[nodiscard]] double rowsPerPerson() const {
    return people == 0 ? 0.0
                       : static_cast<double>(cardRows) /
                             static_cast<double>(people);
  }
};

[[nodiscard]] pl::time::Window window() {
  pl::time::Window w;
  w.start = pl::time::makeTime({1991, 1, 1});
  w.days = kDays;
  return w;
}

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

/// Rank groups by empirical fraud rate — the best a classifier knowing
/// only that one attribute can do — admit them one at a time, and report
/// the best recall reached while precision still clears the floor.
[[nodiscard]] double sweep(const std::vector<Counts> &groupsIn,
                           std::size_t fraudRows, double *bestPrecisionOut) {
  auto groups = groupsIn;
  std::sort(groups.begin(), groups.end(),
            [](const Counts &a, const Counts &b) {
              if (a.rate() != b.rate()) {
                return a.rate() > b.rate();
              }
              return a.total() > b.total();
            });

  std::size_t admittedFraud = 0;
  std::size_t admittedTotal = 0;
  double bestRecall = 0.0;
  double bestPrecision = 0.0;

  for (const auto &g : groups) {
    admittedFraud += g.fraud;
    admittedTotal += g.total();
    if (admittedTotal == 0) {
      continue;
    }
    const double precision = static_cast<double>(admittedFraud) /
                             static_cast<double>(admittedTotal);
    const double recall =
        static_cast<double>(admittedFraud) / static_cast<double>(fraudRows);
    bestPrecision = std::max(bestPrecision, precision);
    if (precision >= kPrecisionFloor) {
      bestRecall = std::max(bestRecall, recall);
    }
  }

  if (bestPrecisionOut != nullptr) {
    *bestPrecisionOut = bestPrecision;
  }
  return bestRecall;
}

} // namespace

int main() {
  try {
    const auto poolSet = pltest::buildPoolSet(kSeed);
    const auto fraudProfile = pltest::scaledFraudProfile();

    std::printf("victim baselines: %d people, %d days from 1991\n",
                kPopulation, kDays);
    std::fflush(stdout);
    const auto result = runWorld(poolSet, fraudProfile);

    // SOURCE -> OWNER, both key spaces. Deposit-account sources resolve
    // through the account registry; cardPurchase sources are CREDIT-CARD
    // keys and resolve through the card registry. Missing the second was
    // the coverage bug this gate now asserts against.
    std::unordered_map<pl::entity::Key, pl::entity::PersonId> accountOwner;
    accountOwner.reserve(result.holdings.accounts.registry.records.size());
    for (const auto &record : result.holdings.accounts.registry.records) {
      accountOwner.emplace(record.id, record.owner);
    }
    const auto ownerOf =
        [&](const pl::entity::Key &key) -> pl::entity::PersonId {
      if (const auto *terms = result.holdings.creditCards.forKey(key)) {
        return terms->owner;
      }
      if (const auto it = accountOwner.find(key); it != accountOwner.end()) {
        return it->second;
      }
      return pl::entity::invalidPerson;
    };

    const auto &pack = result.people.personas;
    const auto population = result.people.roster.roster.count;
    check(!pack.timelines.empty() && !pack.birthDates.empty(),
          "the persona timeline and birth-date carriers must be filled");
    if (g_failures != 0) {
      return 1;
    }

    std::map<personas::Type, Counts> byPersona;  // per-ROW, persona at row
    std::map<int, Counts> byAgeBand;             // per-ROW, age at row
    std::vector<std::uint8_t> isVictim(population + 1, 0);
    std::vector<std::uint32_t> rowsPerPerson(population + 1, 0);

    std::size_t fraudRows = 0;
    std::size_t legitRows = 0;
    std::size_t unresolved = 0;

    for (const auto &t : result.transfers.ledger.posted.txns) {
      if (!inCardView(t)) {
        continue;
      }
      const auto person = ownerOf(t.source);
      if (!pl::entity::valid(person) ||
          static_cast<std::uint32_t>(person) > population) {
        ++unresolved;
        continue;
      }
      const auto idx = static_cast<std::size_t>(person) - 1;
      if (idx >= pack.timelines.size() || idx >= pack.birthDates.size()) {
        ++unresolved;
        continue;
      }

      const auto at = pl::time::fromEpochSeconds(t.timestamp);
      const auto persona = timeline::personaAt(pack.timelines[idx], at);

      const auto born = pack.birthDates[idx];
      const auto now = pl::time::toCalendarDate(at);
      int age = now.year - born.year;
      if (now.month < born.month ||
          (now.month == born.month && now.day < born.day)) {
        --age;
      }
      const int band = (age / kAgeBandYears) * kAgeBandYears;

      ++rowsPerPerson[person];
      if (t.fraud.flag != 0) {
        ++fraudRows;
        ++byPersona[persona].fraud;
        ++byAgeBand[band].fraud;
        isVictim[person] = 1;
      } else {
        ++legitRows;
        ++byPersona[persona].legit;
        ++byAgeBand[band].legit;
      }
    }

    const auto viewRows = fraudRows + legitRows;
    check(fraudRows > 0 && legitRows > 0,
          "the card view must carry both classes (fraud " +
              std::to_string(fraudRows) + ", legit " +
              std::to_string(legitRows) + ")");
    check(byPersona.size() >= 2 && byAgeBand.size() >= 2,
          "at least two persona groups and two age bands must appear");
    if (g_failures != 0) {
      std::fprintf(stderr, "%d precondition(s) failed\n", g_failures);
      return 1;
    }

    // ------------------------------------- INSTRUMENT COVERAGE (gated)
    const double unresolvedShare =
        static_cast<double>(unresolved) /
        static_cast<double>(viewRows + unresolved);
    std::printf("  coverage: %zu card rows attributed, %zu unresolved "
                "(%.4f  <- gate: < %.2f)\n",
                viewRows, unresolved, unresolvedShare, kMaxUnresolvedShare);
    check(unresolvedShare < kMaxUnresolvedShare,
          "the instrument must attribute essentially the whole card view; "
          "unresolved share " +
              std::to_string(unresolvedShare) +
              " means it is measuring a biased subset (the first version "
              "of this gate dropped every credit-card-sourced row)");

    const double baseRate =
        static_cast<double>(fraudRows) / static_cast<double>(viewRows);
    std::printf("  base rate %.5f over %zu card rows\n", baseRate, viewRows);

    // ------------------------- PER-PERSON INCIDENCE (the research axis)
    // Grouped by persona at WINDOW START: a stable per-person cohort
    // label. THIS is what exposure weighting is meant to move, and what
    // the research directions are stated on.
    std::map<personas::Type, Cohort> cohorts;
    for (std::uint32_t person = 1; person <= population; ++person) {
      const auto idx = static_cast<std::size_t>(person) - 1;
      if (idx >= pack.timelines.size()) {
        continue;
      }
      const auto seedPersona =
          timeline::personaAt(pack.timelines[idx], window().start);
      auto &cohort = cohorts[seedPersona];
      ++cohort.people;
      cohort.cardRows += rowsPerPerson[person];
      cohort.victims += isVictim[person] != 0 ? 1 : 0;
    }

    std::size_t totalVictims = 0;
    for (std::uint32_t person = 1; person <= population; ++person) {
      totalVictims += isVictim[person] != 0 ? 1 : 0;
    }
    std::printf("\n  PER-PERSON INCIDENCE over %d years (THE RESEARCH "
                "AXIS — what exposure weighting moves)\n",
                kDays / 365);
    std::printf("    %-14s %7s %8s %10s %12s\n", "persona", "people",
                "victims", "incidence", "rows/person");
    for (const auto &[persona, cohort] : cohorts) {
      std::printf("    %-14s %7zu %8zu %9.3f %12.1f\n",
                  std::string{personas::name(persona)}.c_str(), cohort.people,
                  cohort.victims, cohort.incidence(), cohort.rowsPerPerson());
    }
    std::printf("    ALL            %7u %8zu %9.3f\n", population,
                totalVictims,
                population == 0 ? 0.0
                                : static_cast<double>(totalVictims) /
                                      static_cast<double>(population));

    // ---------------------------- PER-ROW RATES (the shortcut axis)
    std::printf("\n  PER-ROW rates by PERSONA-AT-DATE (the shortcut axis; "
                "~ incidence / rows-per-person)\n");
    std::vector<Counts> personaGroups;
    personaGroups.reserve(byPersona.size());
    for (const auto &[persona, cell] : byPersona) {
      std::printf("    %-14s rows %7zu  fraud %5zu  rate %.5f  (%.2fx "
                  "base)\n",
                  std::string{personas::name(persona)}.c_str(), cell.total(),
                  cell.fraud, cell.rate(),
                  baseRate > 0.0 ? cell.rate() / baseRate : 0.0);
      personaGroups.push_back(cell);
    }
    double personaBestPrecision = 0.0;
    const double personaRecall =
        sweep(personaGroups, fraudRows, &personaBestPrecision);
    std::printf("    best precision %.4f   RECALL @ P>=%.2f  %.4f  "
                "<- gate: < %.2f\n",
                personaBestPrecision, kPrecisionFloor, personaRecall,
                kMaxRecallAtHighPrecision);

    std::printf("\n  PER-ROW rates by AGE-BAND-AT-DATE\n");
    std::vector<Counts> ageGroups;
    ageGroups.reserve(byAgeBand.size());
    for (const auto &[band, cell] : byAgeBand) {
      std::printf("    %3d-%3d        rows %7zu  fraud %5zu  rate %.5f  "
                  "(%.2fx base)\n",
                  band, band + kAgeBandYears - 1, cell.total(), cell.fraud,
                  cell.rate(), baseRate > 0.0 ? cell.rate() / baseRate : 0.0);
      ageGroups.push_back(cell);
    }
    double ageBestPrecision = 0.0;
    const double ageRecall = sweep(ageGroups, fraudRows, &ageBestPrecision);
    std::printf("    best precision %.4f   RECALL @ P>=%.2f  %.4f  "
                "<- gate: < %.2f\n",
                ageBestPrecision, kPrecisionFloor, ageRecall,
                kMaxRecallAtHighPrecision);

    check(personaRecall < kMaxRecallAtHighPrecision,
          "a PERSONA-only rule cannot harvest the corpus: recall at "
          "precision>=0.90 is " +
              std::to_string(personaRecall) + ", gate < " +
              std::to_string(kMaxRecallAtHighPrecision) +
              ". If a victimization tilt pushed this over, the exponent "
              "is wrong — not the gate.");
    check(ageRecall < kMaxRecallAtHighPrecision,
          "an AGE-only rule cannot harvest the corpus: recall at "
          "precision>=0.90 is " +
              std::to_string(ageRecall) + ", gate < " +
              std::to_string(kMaxRecallAtHighPrecision));

    if (g_failures != 0) {
      std::fprintf(stderr, "\n%d victim-baseline gate(s) failed\n",
                   g_failures);
      return 1;
    }
  } catch (const std::exception &e) {
    std::fprintf(stderr, "FAIL: exception: %s\n", e.what());
    return 2;
  }

  std::printf("\ntest_card_victim_baselines: neither persona nor age alone "
              "solves the task\n");
  return 0;
}
