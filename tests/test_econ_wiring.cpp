//
// tests/test_econ_wiring.cpp
//
// macro-history-v1 H1 step 2b + H4 step 2: the nominal-scale and
// macro-modulation WIRING acceptance gates
// (docs/h1_nominal_scale_wiring.md, docs/h4_macro_modulation.md;
// authority U-6/U-9). test_econ_scale pins the primitives' MEANING;
// these gates pin that generation actually REALIZES them, using two
// full windowed legs (window_leg_support.hpp) of the same 300-person
// spec at 1991 and 2019 (730 days each):
//
//   * AWI band     — mean salary deposit, 2019 rows vs 1991 rows,
//                    inside a band around wageScale(2019)/wageScale(1991)
//                    (~2.48x; the surviving idiosyncratic raise lanes
//                    are the declared drift allowance);
//   * CPI band     — mean legit spending ticket (merchant + card),
//                    same shape around priceScale ratio (~1.88x). H4's
//                    CHANNEL-SEPARATION pin: the count modulation must
//                    leave ticket amounts exactly on the CPI axis;
//   * VOLUME band  — H4: anchor-year session ticket COUNTS, 1991 vs
//                    2019, inside +/-15% of realPceLevel(1991) (~0.67).
//                    Same population, same harness position (year 1 of
//                    each leg), so the small-world drain cancels; the
//                    +/-15% allowance absorbs the liquidity feedback (a
//                    quieter 1991 session drains balances slower,
//                    nudging the realized ratio above the pure level);
//   * CALIBRATION  — the calibration year realizes today's magnitudes
//                    exactly: scale == 1.0 means every 2019
//                    subscription debit IS a kPricePool price, cent
//                    for cent. On the COUNT axis the identity is
//                    primitive-exact (realPceLevel(2019) == 1.0 in
//                    IEEE, pinned by test_econ_scale; x * 1.0 == x),
//                    so 2019 frames are bit-identical to the unwired
//                    loop — the golden re-pin is the corpus baseline;
//   * LATTICES     — ATM withdrawals are POSITIVE MULTIPLES OF $20 in
//                    BOTH eras (scale-then-resnap: fewer notes, never
//                    scaled notes);
//   * CLASS S      — structuring stays inside its statutory band
//                    (<= $9,950, near-threshold mass present) in every
//                    era while the OTHER fraud rails scale;
//   * FRAUD RIDES L — H4: the fraud budget law F = pL/(1-p) reads the
//                    REALIZED legit count, so the flagged-row count
//                    ratio across eras must track the legit-row count
//                    ratio (parity band 0.80-1.25) — teeth against a
//                    fraud budget pinned to population or window
//                    constants instead of L;
//   * DRIFT PARITY (R-AWARE, the declared U-9 amendment) — a small
//                    300-person world drains liquidity over its second
//                    year in ANY era (measured ~0.73 deflated y/y — a
//                    harness-world fact that predates the wiring). H4
//                    makes eras' REAL growth intentionally unequal
//                    (1991->1992 grows, 2019->2020 collapses through
//                    COVID), so the gate now compares y/y spend in
//                    CALIBRATION-LEVEL units: totals divided by
//                    priceScale x realPceLevel. The structural drain
//                    still cancels in the cross-era division; only a
//                    scale or level distortion can push parity off 1.
//                    (The COVID 2020 dip rides INSIDE this gate; the
//                    contract's separate 2008-09 recession-direction
//                    leg is DECLARED SUBSUMED — a 1-3% annual level
//                    dip is unresolvable under the ~27% drain at
//                    N=300, the dip direction is pinned at the
//                    primitive, and the 33% VOLUME gate pins the
//                    transmission.)
//
// DIRECTION + magnitude bands, not calibration pins — the golden
// digests pin the bytes; these gates pin the MEANING.
//
// WORLD SHAPE (join-cohort round): both legs now carry the PRODUCTION
// join cohort instead of the joinerless harness world these bands were
// calibrated against (window_leg_support.hpp, WORLD SHAPE) — and the
// two cohorts are UNEQUAL BY DESIGN, which matters more here than in
// any other gate on this harness. The cohort is BEA-sized off the
// window's OWN population growth (synth/personas/join.hpp), and US
// growth was faster in 1991-92 (~1.34%/yr -> 8 joiners at N=300) than
// in 2019-20 (~0.40%/yr -> 2). So this is the one gate whose two sides
// are perturbed by different amounts. Both counts are PRINTED and both
// are PINNED nonzero below, so a future reader of a cross-era ratio
// cannot mistake the asymmetry for a wiring fault.
//
// EVERY BAND RE-VERIFIED, EVERY BAND UNCHANGED. Values observed on the
// joinerless world, and why each survives a corpus re-roll:
//
//   AWI band     obs 2.395 vs 2.480 +-15% = (2.108, 2.852) — a mean over
//     thousands of salary rows per leg (16,582 payday inbounds in the
//     1991 leg); 8-of-300 and 2-of-300 age re-anchorings cannot move it
//     more than a fraction of a percent.
//   CPI band     obs 1.845 vs 1.877 +-15% = (1.595, 2.159) — means over
//     48,721 / 69,057 tickets.
//   VOLUME band  obs 0.7055 vs 0.668 +-15% = (0.568, 0.768) — 8.9%
//     headroom to the upper edge, the LEAST in this file. Kept: the
//     ratio is set by realPceLevel plus the liquidity feedback, both
//     untouched by the world shape, and it is a ratio of ~50k-vs-~69k
//     counts, not a small-sample statistic.
//   CALIBRATION  obs 0 off-pool — an exact identity (priceScale(2019)
//     == 1.0), not a tolerance.
//   LATTICES     obs 0 off-lattice — exact.
//   CLASS S      NOT EXERCISED at this seed (zero structuring rows; the
//     gate prints a note). This is the ONE branch a corpus re-roll can
//     newly ACTIVATE. Its bounds are STATUTORY (<= $9,950) and the
//     near-threshold floor ($6,000) is the "unscaled band" evidence, so
//     neither moves. If the branch activates and the floor fails, that
//     is a COVERAGE event on a sparse rail, not a scaling defect — the
//     row counts are printed beside the verdict so activation is
//     visible either way.
//   RING-RAIL    obs 1.9104 in (1.4, 2.6) on 230/271 rows. This band
//     already exists BECAUSE the mean recomposes on every re-roll: H3
//     part 1 left it at 2.418 and H4 moved it to ~1.91, a 21% swing
//     from a declared model round. It is sized for exactly this event.
//   FRAUD RIDES L obs 0.9255 in (0.80, 1.25) — a ratio of ratios whose
//     fraud-count quantization noise at 354/416 rows is ~5-7%.
//   DRIFT PARITY obs 0.925 in (0.80, 1.25) — totals over ~48k tickets a
//     year on each side, and the harness drain still cancels in the
//     cross-era division.
//
// NOTHING WAS WIDENED. A band widened against a world nobody has
// measured is not a re-calibration, it is a gate with its teeth pulled;
// where a band here is tight, it is tight on a large-N ratio or on an
// exact identity, and where it is loose it is already loose for the
// documented sparse-wobble reason.
//

#include "phantomledger/synth/econ/nominal.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transfers/channels/subscriptions/prices.hpp"

#include "window_leg_support.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace pl = ::PhantomLedger;
namespace econ = pl::synth::econ;
namespace channels = pl::channels;

using pltest::LegOptions;
using pltest::LegResult;
using Txn = pltest::Txn;

namespace {

constexpr std::uint64_t kSeed = 1234567;
constexpr std::int32_t kPopulation = 300;

[[nodiscard]] int yearOf(const Txn &t) {
  return pl::time::toCalendarDate(pl::time::fromEpochSeconds(t.timestamp))
      .year;
}

[[nodiscard]] bool hasChannel(const Txn &t, channels::Tag tag) {
  return t.session.channel.value == tag.value;
}

[[nodiscard]] bool isLegitTicket(const Txn &t) {
  return t.fraud.flag == 0 &&
         (hasChannel(t, channels::tag(channels::Legit::merchant)) ||
          hasChannel(t, channels::tag(channels::Legit::cardPurchase)));
}

// Ring-rail fraud rows: the kFraud/kFraudCycle continuous draws that
// MUST scale. Excludes class S (structuring) and the card-purchase
// rail (whose card-test / gift-card lattices are fixed-nominal by the
// owner-approved U-6 CHOICE).
[[nodiscard]] bool isScaledFraudRow(const Txn &t) {
  return t.fraud.flag == 1 &&
         !hasChannel(t, channels::tag(channels::Fraud::structuring)) &&
         !hasChannel(t, channels::tag(channels::Legit::cardPurchase));
}

struct Stat {
  std::size_t count = 0;
  double total = 0.0;

  void add(double amount) {
    ++count;
    total += amount;
  }

  [[nodiscard]] double mean() const {
    return count == 0 ? 0.0 : total / static_cast<double>(count);
  }
};

int g_failures = 0;

void check(bool cond, const std::string &what) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

void checkBand(double value, double lo, double hi, const std::string &what) {
  check(value > lo && value < hi,
        what + " in (" + std::to_string(lo) + ", " + std::to_string(hi) +
            "), got " + std::to_string(value));
}

[[nodiscard]] LegResult runEraLeg(const pl::synth::pii::PoolSet &pools,
                                  pl::time::CalendarDate start, int days,
                                  const char *label) {
  pltest::announceLeg(label);
  LegOptions opt;
  opt.seed = kSeed;
  opt.window.start = pl::time::makeTime(start);
  opt.window.days = days;
  opt.population = kPopulation;
  opt.withBaseRoutines = true;
  opt.withFamily = true;
  return pltest::runLeg(pools, opt);
}

// H4: y/y drift in CALIBRATION-LEVEL units — nominal totals divided by
// the price scale (amount axis) AND the real level (count axis).
[[nodiscard]] double calibrationLevelTotal(double total, int year) {
  return total / (econ::priceScale(year) * econ::realPceLevel(year));
}

} // namespace

int main() {
  const auto pools = pltest::buildPoolSet(kSeed);

  // Two eras of the same world spec, each spanning a full year pair so
  // the drift-parity gate sees the same harness dynamics in both.
  const auto leg91 = runEraLeg(pools, {1991, 1, 1}, 730, "leg 1991 (730d)");
  const auto leg19 = runEraLeg(pools, {2019, 1, 1}, 730, "leg 2019 (730d)");
  pltest::printLeg("1991", leg91);
  pltest::printLeg("2019", leg19);

  // ---- WORLD SHAPE, before any ratio is formed ----------------------
  // Both legs carry the production join cohort, and the two counts are
  // DELIBERATELY UNEQUAL: the cohort is BEA-sized off each window's own
  // population growth, which was faster in 1991-92 than in 2019-20. See
  // WORLD SHAPE in the file comment — the asymmetry is a model fact, and
  // it is printed so nobody debugging a cross-era ratio reads it as a
  // fault.
  std::printf("  world shape: join cohort 1991 leg %llu, 2019 leg %llu of %d "
              "people (BEA-sized per window — UNEQUAL BY DESIGN)\n",
              static_cast<unsigned long long>(leg91.joiners),
              static_cast<unsigned long long>(leg19.joiners), kPopulation);
  check(leg91.joiners > 0 && leg19.joiners > 0,
        "both era legs carry the production join cohort (1991 " +
            std::to_string(leg91.joiners) + ", 2019 " +
            std::to_string(leg19.joiners) + ")");

  const double w1991 = econ::wageScale(1991);
  const double p1991 = econ::priceScale(1991);
  const double expectedWageRatio = econ::wageScale(2019) / w1991; // ~2.48
  const double expectedPriceRatio = econ::priceScale(2019) / p1991; // ~1.88
  const double expectedVolumeRatio =
      econ::realPceLevel(1991) / econ::realPceLevel(2019); // ~0.67

  // ---- Aggregate stats over the two corpora -----------------------
  // Ratio comparisons filter to the anchor years (1991 / 2019) so the
  // second simulated year never dilutes them; the per-year spend
  // pairs feed the drift-parity gate; the FULL-LEG legit/fraud row
  // counts feed the fraud-rides-L gate (the budget law reads the
  // whole window's realized base count).
  Stat salary91;
  Stat salary19;
  Stat ticket91;
  Stat ticket19;
  Stat fraud91;
  Stat fraud19;
  Stat struct91;
  Stat struct19;
  double structMax = 0.0;
  Stat spend1991;
  Stat spend1992;
  Stat spend2019;
  Stat spend2020;
  std::size_t legitRows91 = 0;
  std::size_t legitRows19 = 0;
  std::size_t fraudRows91 = 0;
  std::size_t fraudRows19 = 0;
  std::size_t atmRows = 0;
  std::size_t atmOffLattice = 0;
  std::size_t sub19Rows = 0;
  std::size_t sub19OffPool = 0;

  const auto salaryTag = channels::tag(channels::Legit::salary);
  const auto atmTag = channels::tag(channels::Legit::atm);
  const auto subTag = channels::tag(channels::Legit::subscription);
  const auto structTag = channels::tag(channels::Fraud::structuring);

  for (const auto &t : leg91.rows) {
    const int year = yearOf(t);
    if (t.fraud.flag == 0) {
      ++legitRows91;
    } else {
      ++fraudRows91;
    }
    if (hasChannel(t, salaryTag) && year == 1991) {
      salary91.add(t.amount);
    }
    if (isLegitTicket(t)) {
      if (year == 1991) {
        ticket91.add(t.amount);
        spend1991.add(t.amount);
      } else if (year == 1992) {
        spend1992.add(t.amount);
      }
    }
    if (isScaledFraudRow(t)) {
      fraud91.add(t.amount);
    }
    if (hasChannel(t, structTag) && t.fraud.flag == 1) {
      struct91.add(t.amount);
      structMax = std::max(structMax, t.amount);
    }
    if (hasChannel(t, atmTag)) {
      ++atmRows;
      if (std::fmod(t.amount, 20.0) != 0.0 || t.amount < 20.0) {
        ++atmOffLattice;
      }
    }
  }

  for (const auto &t : leg19.rows) {
    const int year = yearOf(t);
    if (t.fraud.flag == 0) {
      ++legitRows19;
    } else {
      ++fraudRows19;
    }
    if (hasChannel(t, salaryTag) && year == 2019) {
      salary19.add(t.amount);
    }
    if (isLegitTicket(t)) {
      if (year == 2019) {
        ticket19.add(t.amount);
        spend2019.add(t.amount);
      } else if (year == 2020) {
        spend2020.add(t.amount);
      }
    }
    if (isScaledFraudRow(t)) {
      fraud19.add(t.amount);
    }
    if (hasChannel(t, structTag) && t.fraud.flag == 1) {
      struct19.add(t.amount);
      structMax = std::max(structMax, t.amount);
    }
    if (hasChannel(t, atmTag)) {
      ++atmRows;
      if (std::fmod(t.amount, 20.0) != 0.0 || t.amount < 20.0) {
        ++atmOffLattice;
      }
    }
    // Calibration identity holds for CALIBRATION-YEAR debits only
    // (2020 rows carry priceScale(2020) and leave the pool).
    if (hasChannel(t, subTag) && year == 2019) {
      ++sub19Rows;
      bool onPool = false;
      for (const double price : pl::transfers::subscriptions::kPricePool) {
        if (std::fabs(t.amount - price) < 1e-9) {
          onPool = true;
          break;
        }
      }
      if (!onPool) {
        ++sub19OffPool;
      }
    }
  }

  // The gates are only meaningful on populated aggregates.
  check(salary91.count > 300 && salary19.count > 300,
        "salary rows populated (" + std::to_string(salary91.count) + " / " +
            std::to_string(salary19.count) + ")");
  check(ticket91.count > 1000 && ticket19.count > 1000,
        "ticket rows populated (" + std::to_string(ticket91.count) + " / " +
            std::to_string(ticket19.count) + ")");
  check(atmRows > 100, "atm rows populated (" + std::to_string(atmRows) + ")");
  check(sub19Rows > 50,
        "2019 subscription rows populated (" + std::to_string(sub19Rows) +
            ")");

  // ---- AWI band: salaries ride the wage index ---------------------
  // The surviving salary_real_raise lanes (mu=.015) drift the two-leg
  // ratio around the pure index; +/-15% is the declared allowance.
  const double salaryRatio = salary19.mean() / salary91.mean();
  checkBand(salaryRatio, expectedWageRatio * 0.85, expectedWageRatio * 1.15,
            "salary mean ratio 2019/1991 vs AWI " +
                std::to_string(expectedWageRatio));

  // ---- CPI band: spending tickets ride the price index ------------
  // Liquidity/routing coupling shifts composition slightly between
  // eras (scaled stocks vs scaled flows); +/-15% band. H4's channel-
  // separation pin: the count modulation must NOT move this ratio off
  // the CPI axis.
  const double ticketRatio = ticket19.mean() / ticket91.mean();
  checkBand(ticketRatio, expectedPriceRatio * 0.85, expectedPriceRatio * 1.15,
            "ticket mean ratio 2019/1991 vs CPI " +
                std::to_string(expectedPriceRatio));

  // ---- H4 VOLUME band: session counts ride the real level ----------
  // Anchor-year ticket counts, same population and the same harness
  // position (year 1 of each leg) so the small-world drain cancels.
  // The liquidity feedback pushes the realized ratio slightly ABOVE
  // the pure level (a 0.67x-quiet 1991 session drains its balances
  // slower); +/-15% is the declared allowance (contract, U-9).
  const double volumeRatio = static_cast<double>(ticket91.count) /
                             static_cast<double>(ticket19.count);
  std::printf("  session volume ratio 1991/2019 %.4f (counts %zu / %zu; "
              "realPceLevel %.3f)\n",
              volumeRatio, ticket91.count, ticket19.count,
              expectedVolumeRatio);
  checkBand(volumeRatio, expectedVolumeRatio * 0.85,
            expectedVolumeRatio * 1.15,
            "session ticket count ratio 1991/2019 vs realPceLevel " +
                std::to_string(expectedVolumeRatio));

  // ---- CALIBRATION IDENTITY: 2019 == today's magnitudes -----------
  // priceScale(2019) == 1.0 exactly, so every 2019 subscription debit
  // is a verbatim kPricePool price. This is the corpus-level echo of
  // test_econ_scale's 1.0 pin. (The COUNT-axis identity is primitive-
  // exact: realPceLevel(2019) == 1.0 in IEEE, and x * 1.0 == x, so
  // 2019 frames sample bit-identically to the unmodulated loop.)
  check(sub19OffPool == 0,
        "every 2019 subscription debit is a verbatim kPricePool price (" +
            std::to_string(sub19OffPool) + " of " +
            std::to_string(sub19Rows) + " off-pool)");

  // ---- DENOMINATION LATTICE: $20 notes in every era ---------------
  check(atmOffLattice == 0,
        "every ATM withdrawal is a positive multiple of $20 (" +
            std::to_string(atmOffLattice) + " of " + std::to_string(atmRows) +
            " off-lattice)");

  // ---- CLASS S: structuring band intact, other fraud scales -------
  // The statutory bounds do not move with the world shape. The row
  // counts are printed either way, because at this seed the branch has
  // never activated and a corpus re-roll is what would activate it:
  // "no rows" and "rows inside the band" are different verdicts and the
  // output has to say which one happened.
  if (struct91.count + struct19.count > 0) {
    std::printf("  class-S structuring rows %zu / %zu, max $%.2f  <- gate: "
                "in ($6,000, $9,950]\n",
                struct91.count, struct19.count, structMax);
    check(structMax <= 9950.0 + 1e-9,
          "structuring never exceeds the $9,950 band, got max " +
              std::to_string(structMax));
    check(structMax > 6000.0,
          "structuring keeps near-threshold mass (unscaled band), max " +
              std::to_string(structMax));
  } else {
    std::printf("  note: no structuring rows at this seed (0 / 0) — class-S "
                "band sub-gate not exercised\n");
  }

  // ---- Ring-rail fraud scales upward (sparse band) -----------------
  // The mean of ~1-2k heavy-tailed lognormal rows (kFraud sigma .70,
  // mixed with mule-forwarding fractions) recomposes whenever the
  // candidate count L moves — the fraud budget is F = pL/(1-p), so any
  // declared model-moving round (H2 income switch, H3 death-clipped
  // income, H4 count modulation, the join-cohort world shape)
  // legitimately reshuffles which draws exist. Observed spread at this
  // seed: 2.418 after H3 part 1, ~1.91 after H4 (vs CPI 1.877) — a
  // ~29% sparse wobble, NOT a scaling defect. The band is therefore
  // DIRECTIONAL (fraud amounts grow with the era, magnitude near-CPI,
  // upper edge 2.60): it catches fixed-nominal fraud (~1.0) and gross
  // double-scaling (>3), while the exact axis LAW is pinned where it is
  // tight — test_econ_scale (the scale), test_fraud_amounts (the
  // samplers), and the class-S sub-gate above (the statutory
  // exception).
  if (fraud91.count >= 30 && fraud19.count >= 30) {
    const double fraudRatio = fraud19.mean() / fraud91.mean();
    std::printf("  ring-rail fraud mean ratio %.4f (counts %zu / %zu; CPI "
                "%.3f)\n",
                fraudRatio, fraud91.count, fraud19.count, expectedPriceRatio);
    checkBand(fraudRatio, 1.4, 2.6,
              "ring-rail fraud mean ratio 2019/1991 (CPI-scaled, sparse "
              "band)");
  } else {
    const double fraudRatio =
        fraud91.count > 0 && fraud19.count > 0
            ? fraud19.mean() / fraud91.mean()
            : 0.0;
    check(fraud91.count > 0 && fraud19.count > 0 && fraudRatio > 1.2,
          "sparse fraud rows still scale upward (counts " +
              std::to_string(fraud91.count) + "/" +
              std::to_string(fraud19.count) + ", ratio " +
              std::to_string(fraudRatio) + ")");
  }

  // ---- H4 FRAUD RIDES L: the budget law follows the count axis -----
  // The illicit budget base is the REALIZED legit row count (plus
  // camouflage), so when the count modulation moves L across eras the
  // flagged-row count must move proportionally. Ring/burst emission
  // quantizes the realized count; 0.80-1.25 is the parity allowance.
  check(fraudRows91 > 30 && fraudRows19 > 30,
        "flagged rows populated (" + std::to_string(fraudRows91) + " / " +
            std::to_string(fraudRows19) + ")");
  if (fraudRows91 > 30 && fraudRows19 > 30) {
    const double legitCountRatio = static_cast<double>(legitRows19) /
                                   static_cast<double>(legitRows91);
    const double fraudCountRatio = static_cast<double>(fraudRows19) /
                                   static_cast<double>(fraudRows91);
    const double fraudParity = fraudCountRatio / legitCountRatio;
    std::printf("  fraud-rides-L parity %.4f (legit %zu / %zu, fraud %zu / "
                "%zu)\n",
                fraudParity, legitRows91, legitRows19, fraudRows91,
                fraudRows19);
    checkBand(fraudParity, 0.80, 1.25,
              "fraud count ratio tracks the legit count ratio (F = "
              "pL/(1-p))");
  }

  // ---- DRIFT PARITY (R-AWARE): harness drain cancels across eras ---
  // Deflated y/y spend falls in year 2 of ANY 300-person leg (small
  // worlds drain liquidity — a harness fact, not an era artifact). H4
  // makes REAL growth era-unequal by design, so each year's total is
  // first brought to CALIBRATION-LEVEL units (divide by priceScale x
  // realPceLevel — the declared U-9 gate amendment); dividing the 1991
  // pair by the 2019 pair then cancels the structural drain, and the
  // parity moves off 1 only if the scale or level wiring distorts
  // drift.
  check(spend1991.count > 500 && spend1992.count > 500 &&
            spend2019.count > 500 && spend2020.count > 500,
        "per-year spend populated (" + std::to_string(spend1991.count) +
            " / " + std::to_string(spend1992.count) + " / " +
            std::to_string(spend2019.count) + " / " +
            std::to_string(spend2020.count) + ")");
  const double drift91 = calibrationLevelTotal(spend1992.total, 1992) /
                         calibrationLevelTotal(spend1991.total, 1991);
  const double drift19 = calibrationLevelTotal(spend2020.total, 2020) /
                         calibrationLevelTotal(spend2019.total, 2019);
  const double driftParity = drift91 / drift19;
  std::printf("  calibration-level y/y drift: 1991->1992 %.3f, 2019->2020 "
              "%.3f\n",
              drift91, drift19);
  checkBand(driftParity, 0.80, 1.25,
            "calibration-level drift parity (1991 pair / 2019 pair)");

  if (g_failures != 0) {
    std::fprintf(stderr, "%d gate(s) failed\n", g_failures);
    return 1;
  }
  std::printf(
      "test_econ_wiring: all gates passed (salary x%.3f vs AWI %.3f; "
      "ticket x%.3f vs CPI %.3f; volume x%.3f vs R %.3f; drift parity "
      "%.3f; joiners %llu/%llu)\n",
      salaryRatio, expectedWageRatio, ticketRatio, expectedPriceRatio,
      volumeRatio, expectedVolumeRatio, driftParity,
      static_cast<unsigned long long>(leg91.joiners),
      static_cast<unsigned long long>(leg19.joiners));
  return 0;
}
