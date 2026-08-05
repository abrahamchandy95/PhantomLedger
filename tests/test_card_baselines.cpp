//
// tests/test_card_baselines.cpp
//
// card-fraud-realism-v2 step c: THE MERCHANT-ID-ONLY BASELINE —
// minimum-realism gate 5 of docs/card_fraud_online_gnn.md:
//
//   "A merchant-ID-only baseline must not solve the task."
//
// test_card_merchant_overlap measures whether fraud and legitimate
// rows SHARE merchants. That is necessary but not sufficient: overlap
// could be nonzero while fraud still concentrates so hard on a few
// identifiable merchants that a lookup table wins. This gate answers
// the question a modeller actually cares about — how much fraud can a
// classifier that knows NOTHING BUT THE MERCHANT ID capture at high
// precision?
//
// THE METRIC (standard, and deliberately not PR-AUC, which is
// unstable at these row counts): score every card-rail row by its
// merchant's empirical fraud rate, sweep the threshold, and report
//
//   * RECALL AT PRECISION >= 0.90 — the share of all fraud a
//     high-precision merchant-lookup rule can harvest. This is the
//     number that decides whether the shortcut is real: a rule that
//     grabs most of the fraud at 90% precision IS the task, solved,
//     without behavior.
//   * the pure-group share — fraud sitting on merchants that carry no
//     legitimate row at all (the degenerate case).
//   * LIFT — best achievable precision over the base rate.
//
// GATE: recall at precision >= 0.90 must stay BELOW kMaxRecallAtHighPrecision.
// Some signal is expected and correct — fraud genuinely prefers some
// merchant categories, and a real detector uses that. What must not
// exist is a lookup that harvests the corpus.
//
// SCOPE NOTE: the home-area-only and IP-only baselines named in the
// roadmap are NOT here. The home-area baseline measures the
// Bettencourt victimization tilt, which has not landed yet (step b'),
// so today it would test nothing; it ships WITH b'. The IP baseline
// needs a /24 accessor and must be designed not to flag deliberate
// ring infrastructure sharing as a leak — that is real behavioral
// signal, not a shortcut. Both are tracked in
// docs/card_fraud_v2_roadmap.md.
//
// WORLD SHAPE (join-cohort round): the leg now carries the PRODUCTION
// join cohort — 8 joiners at 300 people over 730 days from 1991, by the
// BEA sizing in synth/personas/join.hpp — instead of the joinerless
// harness world this gate was calibrated against
// (window_leg_support.hpp, WORLD SHAPE). BOTH BANDS RE-VERIFIED, BOTH
// UNCHANGED, and the reason is that neither is a statistical tolerance:
//
//   * recall @ P>=0.90 < 0.25. The bound is held by the b-2 mechanism,
//     not by a seed: fraud draws its card-rail destinations from the
//     same merchant acceptance population the legitimate session draws
//     from, and that population carries ~167 legitimate rows per
//     merchant at this leg size (42,369 over 254). A 0.90-precision
//     group needs ~9 fraud rows against at most 1 legitimate row on the
//     same merchant. Re-anchoring 8 of 300 people's ages and lifespans
//     re-rolls WHICH rows exist; it does not change the destination
//     population, so it cannot manufacture that group.
//   * pure-fraud-merchant share < 0.10. Same mechanism: a fraud-touched
//     merchant with zero legitimate rows is exactly what b-2 removed
//     structurally.
//
// THE OBSERVED FIGURES THAT USED TO SIT HERE WERE STALE AND ONE OF THEM
// WAS A FALSE SAFETY ARGUMENT — corrected 2026-08, the world having moved
// under them twice (the join-cohort flip, then merchant-selection-2026-08).
// This block read "Observed 0.0000" for both bands and, worse, "the
// sweep's BEST precision at ANY threshold is 0.1111 — it never comes near
// the 0.90 floor". The binary PRINTS the truth on every run and disagreed:
//
//     pure-fraud-merchant share      0.0079   (1 fraud-only of 41 touched)
//     best precision (any threshold) 1.0000   (lift 295.65x)
//     RECALL @ precision>=0.90       0.0079
//
// Precision DOES reach 1.0 — a merchant whose only rows are fraud scores
// 1.0 trivially, and at this leg size one exists. The band is therefore
// held by RECALL being tiny (0.0079 of fraud sits on such merchants), NOT
// by precision being unreachable. Both bands still pass and neither was
// widened; what was wrong was the argument for why they are safe.
//
// A LARGER LEG HAS LESS ROOM THAN THIS ONE. At pop 900 x 1461d the same
// sweep measures pureShare 0.0661 against the 0.10 band — 6 fraud-only
// merchants of 153. Anyone sizing a new merchant-side band against the
// 0.0000 that used to be written here would believe they had four times
// the headroom they have. DO NOT RESTATE A PRINTED NUMBER IN A COMMENT:
// read it off the run.
//
// The world shape is now PINNED below, so this baseline can never again
// be measured against a population production does not generate.
//

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"

#include "window_leg_support.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace pl = ::PhantomLedger;
namespace channels = pl::channels;

using pltest::LegOptions;
using Txn = pltest::Txn;

namespace {

constexpr std::uint64_t kSeed = 1234567;
constexpr std::int32_t kPopulation = 300;

// A merchant-lookup rule may not harvest more than this share of all
// fraud at 90% precision. Sized well above "no signal" (fraud does
// prefer some merchants) and well below "solved".
constexpr double kMaxRecallAtHighPrecision = 0.25;
constexpr double kPrecisionFloor = 0.90;

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

int g_failures = 0;

void check(bool cond, const std::string &what) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

[[nodiscard]] bool isCardRail(const Txn &t) {
  return t.session.channel.value ==
         channels::tag(channels::Legit::cardPurchase).value;
}

} // namespace

int main() {
  const auto pools = pltest::buildPoolSet(kSeed);

  pltest::announceLeg("card baseline leg (1991, 730d)");
  LegOptions opt;
  opt.seed = kSeed;
  opt.window.start = pl::time::makeTime(pl::time::CalendarDate{1991, 1, 1});
  opt.window.days = 730;
  opt.population = kPopulation;
  opt.withBaseRoutines = true;
  opt.withFamily = true;
  const auto leg = pltest::runLeg(pools, opt);
  pltest::printLeg("card-baselines", leg);

  // WORLD SHAPE, asserted before anything is measured: an anti-shortcut
  // baseline is only evidence about the shipped corpus if it ran on the
  // shipped population. A zero here means the leg rebuilt the
  // pre-H3-3c-ii joinerless world.
  check(leg.joiners > 0,
        "the leg carries the production join cohort (joiners " +
            std::to_string(leg.joiners) + ")");

  std::map<pl::entity::Key, Counts> byMerchant;
  std::size_t fraudRows = 0;
  std::size_t legitRows = 0;

  for (const auto &t : leg.rows) {
    if (!isCardRail(t)) {
      continue;
    }
    auto &cell = byMerchant[t.target];
    if (t.fraud.flag == 1) {
      ++cell.fraud;
      ++fraudRows;
    } else {
      ++cell.legit;
      ++legitRows;
    }
  }

  const auto totalRows = fraudRows + legitRows;
  check(fraudRows > 0 && legitRows > 0 && byMerchant.size() > 1,
        "card rail populated for a baseline (fraud " +
            std::to_string(fraudRows) + ", legit " +
            std::to_string(legitRows) + ", merchants " +
            std::to_string(byMerchant.size()) + ")");
  if (g_failures != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
  }

  const double baseRate =
      static_cast<double>(fraudRows) / static_cast<double>(totalRows);

  // THE SWEEP. Rank merchants by empirical fraud rate (the best a
  // merchant-ID-only classifier can do — it has no other signal), then
  // admit them one group at a time, tracking precision and recall of
  // the admitted set. The best recall seen while precision is still at
  // or above the floor is the answer.
  std::vector<Counts> groups;
  groups.reserve(byMerchant.size());
  for (const auto &[key, cell] : byMerchant) {
    (void)key;
    groups.push_back(cell);
  }
  std::sort(groups.begin(), groups.end(),
            [](const Counts &a, const Counts &b) {
              if (a.rate() != b.rate()) {
                return a.rate() > b.rate();
              }
              return a.total() > b.total();
            });

  std::size_t admittedFraud = 0;
  std::size_t admittedTotal = 0;
  double bestRecallAtPrecision = 0.0;
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
      bestRecallAtPrecision = std::max(bestRecallAtPrecision, recall);
    }
  }

  // The degenerate case, kept visible: fraud on merchants with no
  // legitimate row at all.
  std::size_t pureFraudRows = 0;
  for (const auto &g : groups) {
    if (g.fraud > 0 && g.legit == 0) {
      pureFraudRows += g.fraud;
    }
  }
  const double pureShare =
      static_cast<double>(pureFraudRows) / static_cast<double>(fraudRows);

  std::printf("  world shape: join cohort %llu of %d people\n",
              static_cast<unsigned long long>(leg.joiners), kPopulation);
  std::printf("  base rate %.5f over %zu card rows (%zu merchants)\n",
              baseRate, totalRows, byMerchant.size());
  std::printf("  pure-fraud-merchant share      %.4f\n", pureShare);
  std::printf("  best precision (any threshold) %.4f  (lift %.2fx)\n",
              bestPrecision,
              baseRate > 0.0 ? bestPrecision / baseRate : 0.0);
  std::printf("  RECALL @ precision>=%.2f        %.4f  <- gate: < %.2f\n",
              kPrecisionFloor, bestRecallAtPrecision,
              kMaxRecallAtHighPrecision);

  check(bestRecallAtPrecision < kMaxRecallAtHighPrecision,
        "a merchant-ID-only rule cannot harvest the corpus: recall at "
        "precision>=" +
            std::to_string(kPrecisionFloor) + " is " +
            std::to_string(bestRecallAtPrecision) + ", gate < " +
            std::to_string(kMaxRecallAtHighPrecision));

  check(pureShare < 0.10,
        "fraud does not concentrate on merchants with zero legitimate "
        "rows (share " +
            std::to_string(pureShare) + ")");

  if (g_failures != 0) {
    std::fprintf(stderr, "%d gate(s) failed\n", g_failures);
    return 1;
  }
  std::printf("test_card_baselines: merchant-ID-only baseline does NOT "
              "solve the task (recall@P%.2f %.4f)\n",
              kPrecisionFloor, bestRecallAtPrecision);
  return 0;
}
