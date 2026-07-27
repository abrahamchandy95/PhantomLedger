//
// tests/test_card_class_f.cpp
//
// U-6 CLASS F ON THE CARD RAIL — the replacement instrument
// (roadmap item e; authority U-13 ADDENDUM 2 registered it when the
// previous attempt was withdrawn, ADDENDUM 3 records the measured run).
//
// THE CLAIM: `cardFraudSpend` is a CONTINUOUS sampler, so authority U-6
// class F applies — its realized amounts must ride the era's price
// level. `unauthorized.cpp` is explicit that priceScale reaches "the
// continuous samplers (cardFraudSpend, atoDrainAmount)" while "the
// denomination samplers (cardTestCharge, giftCardScamAmount) stay
// FIXED-NOMINAL — owner-approved lattice CHOICE".
//
// WHY THIS GATE EXISTS AT ALL: test_card_prevalence used to assert the
// claim by deflating the card view's per-year mean fraud amount and
// requiring flatness. That gate was WITHDRAWN, not widened, for two
// independent reasons, and both of them shape this file:
//
//   (1) MIS-SPECIFIED. The card view mixes THREE amount families — two
//       fixed-nominal lattices and one CPI-scaled continuous sampler —
//       so deflating the COMBINED mean asserts the OPPOSITE of U-6 for
//       two thirds of them. (test_econ_wiring's isScaledFraudRow() had
//       already excluded this rail for exactly that reason; the two
//       gates contradicted each other.)
//   (2) UNDER-POWERED. A per-year mean of 42-92 draws from
//       lognormal(sigma 1.2) carries CV = 1.79/sqrt(n) = 19-28%, and the
//       gate's whole envelope was 2.5x. Null and alternative were not
//       separable at the available n, so the band was decoration.
//
// The withdrawn gate's own decomposition, now printed by
// test_card_prevalence, confirms (2) directly: purging the RESOLVABLE
// lattice made the flatness spread WORSE, not better (nominal-all 2.47x,
// deflated-all 2.69x, deflated CPI-SCALED-ONLY 3.11x). A statistic that
// degrades when you remove a contaminant is being driven by sampling
// noise, not by the mixture.
//
// ------------------------------------------------------------------
// HOW THIS INSTRUMENT AVOIDS BOTH
// ------------------------------------------------------------------
//
// A. THE CONTAMINATION IS SPLIT INTO RESOLVABLE AND UNRESOLVABLE, and
//    handled differently, because only one of them CAN be.
//
//    * giftCardScamAmount IS resolvable: those rows carry
//      FraudType::scamGiftCard. They are EXCLUDED outright. They also
//      sit HIGH on the amount axis ($50-$500, 75% of the mass on
//      {100,200,500,500,500}), well above the 1991 spend median of ~$42,
//      so leaving them in would corrupt an upper quantile badly.
//    * cardTestCharge is NOT resolvable. A probe carries the same
//      txnFraudSolo / txnFraudRing type as the spend it precedes, and
//      nothing on a Transaction says "this was the $1 test". There is no
//      filter to write.
//
// B. SO THE STATISTIC IS CHOSEN TO BE IMMUNE TO WHAT CANNOT BE FILTERED.
//    For a positive scale s, if X = s * X0 then Q_p(X) = s * Q_p(X0) for
//    EVERY p — a quantile reads the scale exactly, and the choice of p
//    is free. The unresolvable probes are bounded at $5.00 nominal in
//    every era, i.e. they occupy the very bottom of the amount axis, so
//    any quantile ABOVE their mass share never sees them. p = 0.75.
//
//    That is the design lesson from the withdrawn gate, and it is the
//    reason this one can exist: when a mixture cannot be separated,
//    do not deflate the aggregate — pick a statistic the mixture's
//    unresolvable component cannot reach, and then GATE that the
//    component really is below it (kMaxProbeSuspectShare, below).
//
// C. THE COMPARISON IS CROSS-ERA, NOT WITHIN-ERA. The withdrawn gate
//    asked whether four per-year means were FLAT, i.e. it tried to
//    resolve a null effect against heavy-tailed noise. This one asks
//    whether the amount distribution MOVED BY THE PRICE LEVEL between
//    1991 and 2019 — an effect of ~1.8x against the same noise. Same
//    shape as test_econ_wiring's ring-rail gate, which works for the
//    same reason: the cross-era division cancels the mixture instead of
//    riding it.
//
// D. EACH ROW IS DEFLATED BY ITS OWN YEAR'S priceScale before the
//    quantile is taken, so the multi-year legs below carry no residual
//    intra-leg scale smear (1991-94 spans 0.5327 -> 0.5798, an 8.8%
//    spread that a single priceScale(startYear) would have swallowed
//    silently). After deflation each leg is a single scale family and
//    the cross-era ratio of deflated quantiles is 1.0 UNDER THE CLAIM.
//    The NOMINAL ratio is printed beside it for direction and legibility
//    — the standing law that a band ships with its discriminator.
//
// ------------------------------------------------------------------
// THE BAND SIZES ITSELF, AND THE GATE CHECKS ITS OWN POWER
// ------------------------------------------------------------------
//
// A hand-picked envelope is what failed last time, so this file does not
// pick one. The asymptotic relative standard error of a sample
// p-quantile of a lognormal is
//
//     CV = sigma * sqrt(p(1-p)/n) / phi(z_p)
//        = 1.2 * sqrt(0.1875/n) / 0.31780   (p = 0.75)
//        = 1.635 / sqrt(n)
//
// so the band is +-kSigmas of the REALIZED two-leg standard error, in
// log space. It therefore tightens when the world gives more rows and
// widens when it gives fewer, instead of encoding one seed's luck.
//
// THEN THE INSTRUMENT GATES ITS OWN COVERAGE — the part that makes this
// more than a re-parameterisation. Two nulls are computed from the
// realized price scales:
//
//     FIXED-NOMINAL   (the defect this gate exists to catch): deflated
//                     ratio collapses to meanScale91 / meanScale19
//     DOUBLE-SCALED   (priceScale applied twice): its reciprocal
//
// and the gate FAILS if its own band fails to exclude either of them.
// An under-powered run reports UNDER-POWERED rather than passing
// vacuously, and the repair is a LARGER LEG, never a tighter band. This
// is the "a measurement instrument must GATE ITS OWN COVERAGE" law
// applied to the exact failure mode that killed the previous gate.
//
// ------------------------------------------------------------------
// WORLD SHAPE, COST, AND WHAT THE FIRST RUN MEASURED
// ------------------------------------------------------------------
//
// Both legs carry the production join cohort by default (authority U-13)
// and pin it nonzero before any ratio is formed. Measured: 45 joiners on
// the 1991 leg, 5 on the 2019 leg.
//
// FIRST GREEN RUN (5.1s, seed 1234567):
//
//   1991  n=666  meanScale 0.5536  nominal Q75 $74.62   deflated $134.19
//   2019  n=431  meanScale 1.0065  nominal Q75 $120.78  deflated $120.03
//   CV 0.0634 / 0.0788 -> SE 0.1011 -> band (0.7384, 1.3542)
//   DEFLATED Q75 ratio 0.8945  (1.1 sigma from 1.0 — unremarkable)
//   nulls excluded: FIXED-NOMINAL 0.5500 at 2.9 sigma, DOUBLE-SCALED
//   1.8181 at 3.0 sigma
//
// POPULATION 900 IS DELIBERATE AND COSTS RUNTIME. The card rail is
// sparse — the fraud budget F = pL/(1-p) puts only a few hundred rows on
// it per leg — and the power arithmetic above is not satisfiable at the
// N=300 the other card gates use: n would be ~222/144, SE ~0.175, band
// ~(0.59, 1.69), which clears the fixed-nominal null by a margin too
// thin to call coverage. A cheaper leg would reproduce precisely the
// defect this file replaces.
//
// CORRECTION, from the first run (this file's own claim, falsified by
// its own output): an earlier draft justified N=900 partly as clearing
// the ~833 threshold where rings exist, "so both txnFraudSolo and
// txnFraudRing card spends are exercised". MEASURED: ring 0 in BOTH
// legs. That was wrong on mechanism, not just on count —
// buildCompromisePlans excludes ring participants and ring victims
// (docs/card_fraud_victimization.md F2), so the unauthorized card rail
// is ring-free BY DESIGN and its class-F population is txnFraudSolo
// only. N=900 stands on the power arithmetic alone. The ring and
// "other" counters are RETAINED as a tripwire: if a future round ever
// routes ring or scam fraud onto the card rail, they stop reading zero
// and the population this gate measures has silently changed.
//
// WINDOWS ARE DELIBERATELY UNEQUAL (1461d at 1991, 730d at 2019). This
// gate compares amount DISTRIBUTIONS, not counts, so the windows need
// not match — and the 2019 leg cannot be extended, because the era lock
// ends the corpus window at 2021-01-01. The 1991 leg is run long instead
// to buy back the statistical power the short modern leg cannot.
//
// MEMORY: the pair peaks around 1.4 GB, the largest in the suite. That
// is the 1461-day N=900 leg's replay-ready buffers, not a leak.
//

#include "phantomledger/synth/econ/nominal.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/fraud/types.hpp"

#include "window_leg_support.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
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
constexpr std::int32_t kPopulation = 900;

// The quantile, and its analytic sampling CV factor for lognormal
// sigma = 1.2 (see the header derivation). Both are stated here so a
// future reader changing one remembers to re-derive the other.
constexpr double kQuantile = 0.75;
constexpr double kQuantileCvFactor = 1.635;
constexpr double kSigmas = 3.0;

// The unresolvable card-test probes must stay well below the quantile
// or it stops being a clean read on the CPI-scaled family. Estimated
// conservatively as "rows at or below the probe ceiling", which also
// counts the genuine low tail of cardFraudSpend and therefore
// OVER-states the contamination — the safe direction for a coverage
// check.
//
// MEASURED on the first run: 0.2462 (1991) and 0.2181 (2019), i.e. a 2x
// margin to this bound rather than the ~3.5x an earlier estimate
// assumed. The bound is NOT tightened toward the observation: the
// failure direction is already safe. If the probe share ever climbed
// past the quantile, Q75 would start reading the fixed-nominal lattice
// and the deflated ratio would collapse toward the FIXED-NOMINAL null —
// a loud FALSE ALARM, not a silent false pass.
constexpr double kProbeCeiling = 5.0;
constexpr double kMaxProbeSuspectShare = 0.50;

// Below this the coverage verdict is reported as a row-count failure
// rather than as a band failure, so a sparse world never reads as a
// scaling defect.
constexpr std::size_t kMinRowsPerLeg = 200;

int g_failures = 0;

void check(bool cond, const std::string &what) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

[[nodiscard]] int yearOf(const Txn &t) {
  return pl::time::toCalendarDate(pl::time::fromEpochSeconds(t.timestamp))
      .year;
}

[[nodiscard]] bool isCardRail(const Txn &t) {
  return t.session.channel.value ==
         channels::tag(channels::Legit::cardPurchase).value;
}

// The RESOLVABLE fixed-nominal lattice. cardTestCharge is deliberately
// absent: it is not resolvable from a Transaction, and the quantile is
// what handles it. See the header.
[[nodiscard]] bool isFixedNominalLattice(const Txn &t) {
  return t.fraud.type == pl::fraud::FraudType::scamGiftCard;
}

// Linear interpolation between order statistics; the vector must be
// sorted ascending.
[[nodiscard]] double quantileOf(const std::vector<double> &sorted, double p) {
  if (sorted.empty()) {
    return 0.0;
  }
  if (sorted.size() == 1) {
    return sorted.front();
  }
  const double pos = p * static_cast<double>(sorted.size() - 1);
  const auto lo = static_cast<std::size_t>(pos);
  const std::size_t hi = std::min(lo + 1, sorted.size() - 1);
  const double frac = pos - static_cast<double>(lo);
  return sorted[lo] * (1.0 - frac) + sorted[hi] * frac;
}

// One era leg's class-F card-rail population.
struct RailSample {
  std::vector<double> nominal;
  std::vector<double> deflated;
  double scaleTotal = 0.0;   // row-weighted, for the null estimates
  double nominalTotal = 0.0; // for the printed mean discriminator
  std::size_t giftCardRows = 0;
  std::size_t probeSuspect = 0;
  std::size_t solo = 0;
  // TRIPWIRE, not dead code: both read 0 today because the unauthorized
  // card rail excludes ring participants by construction. A nonzero
  // reading means a later round routed a different fraud family onto
  // this rail and the measured population is no longer what the header
  // describes.
  std::size_t ring = 0;
  std::size_t other = 0;

  [[nodiscard]] std::size_t count() const { return nominal.size(); }

  [[nodiscard]] double meanScale() const {
    return count() == 0 ? 0.0 : scaleTotal / static_cast<double>(count());
  }

  [[nodiscard]] double mean() const {
    return count() == 0 ? 0.0 : nominalTotal / static_cast<double>(count());
  }

  [[nodiscard]] double probeShare() const {
    return count() == 0 ? 0.0
                        : static_cast<double>(probeSuspect) /
                              static_cast<double>(count());
  }

  // CV of the sample quantile, from the analytic factor above.
  [[nodiscard]] double quantileCv() const {
    return count() == 0 ? 1.0
                        : kQuantileCvFactor /
                              std::sqrt(static_cast<double>(count()));
  }

  void sortInPlace() {
    std::sort(nominal.begin(), nominal.end());
    std::sort(deflated.begin(), deflated.end());
  }
};

[[nodiscard]] RailSample collect(const LegResult &leg) {
  RailSample out;
  for (const auto &t : leg.rows) {
    if (t.fraud.flag != 1 || !isCardRail(t)) {
      continue;
    }
    if (isFixedNominalLattice(t)) {
      ++out.giftCardRows;
      continue;
    }
    const double scale = econ::priceScale(yearOf(t));
    out.nominal.push_back(t.amount);
    out.deflated.push_back(t.amount / scale);
    out.scaleTotal += scale;
    out.nominalTotal += t.amount;
    if (t.amount <= kProbeCeiling) {
      ++out.probeSuspect;
    }
    switch (t.fraud.type) {
    case pl::fraud::FraudType::txnFraudSolo:
      ++out.solo;
      break;
    case pl::fraud::FraudType::txnFraudRing:
      ++out.ring;
      break;
    default:
      ++out.other;
      break;
    }
  }
  out.sortInPlace();
  return out;
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

void report(const char *label, const RailSample &s) {
  std::printf("  %s: class-F card rows %zu (solo %zu, ring %zu, other %zu; "
              "gift-card lattice %zu EXCLUDED)\n",
              label, s.count(), s.solo, s.ring, s.other, s.giftCardRows);
  std::printf("      mean scale %.4f | nominal Q%.0f $%.2f | deflated Q%.0f "
              "$%.2f | nominal mean $%.2f\n",
              s.meanScale(), kQuantile * 100.0,
              quantileOf(s.nominal, kQuantile), kQuantile * 100.0,
              quantileOf(s.deflated, kQuantile), s.mean());
  std::printf("      probe-suspect rows (<= $%.2f) %zu = %.4f of the leg "
              "<- must stay < %.2f for the quantile to be a clean read\n",
              kProbeCeiling, s.probeSuspect, s.probeShare(),
              kMaxProbeSuspectShare);
}

} // namespace

int main() {
  const auto pools = pltest::buildPoolSet(kSeed);

  // Unequal windows BY DESIGN — see the header. The 2019 leg stops at
  // the era lock (2021-01-01); the 1991 leg runs long to buy back power.
  const auto leg91 =
      runEraLeg(pools, {1991, 1, 1}, 1461, "class-F leg 1991 (1461d)");
  const auto leg19 =
      runEraLeg(pools, {2019, 1, 1}, 730, "class-F leg 2019 (730d)");
  pltest::printLeg("1991", leg91);
  pltest::printLeg("2019", leg19);

  // ---- WORLD SHAPE, before any ratio is formed --------------------
  // Authority U-13: a band is only evidence about the shipped corpus if
  // it was measured on the shipped population.
  std::printf("  world shape: join cohort 1991 leg %llu, 2019 leg %llu of %d "
              "people\n",
              static_cast<unsigned long long>(leg91.joiners),
              static_cast<unsigned long long>(leg19.joiners), kPopulation);
  check(leg91.joiners > 0 && leg19.joiners > 0,
        "both era legs carry the production join cohort (1991 " +
            std::to_string(leg91.joiners) + ", 2019 " +
            std::to_string(leg19.joiners) + ")");

  const auto s91 = collect(leg91);
  const auto s19 = collect(leg19);
  report("1991", s91);
  report("2019", s19);

  // ---- COVERAGE, gated before the claim ---------------------------
  // A sparse rail must never be reported as a scaling defect.
  check(s91.count() >= kMinRowsPerLeg && s19.count() >= kMinRowsPerLeg,
        "class-F card rows populated in both eras (" +
            std::to_string(s91.count()) + " / " + std::to_string(s19.count()) +
            ", need " + std::to_string(kMinRowsPerLeg) + " each)");

  // The quantile is only a clean read on the CPI-scaled family while the
  // unresolvable probes sit below it.
  check(s91.probeShare() < kMaxProbeSuspectShare &&
            s19.probeShare() < kMaxProbeSuspectShare,
        "fixed-nominal probes stay below the Q" +
            std::to_string(static_cast<int>(kQuantile * 100)) +
            " read (shares " + std::to_string(s91.probeShare()) + " / " +
            std::to_string(s19.probeShare()) + ")");

  if (g_failures != 0) {
    std::fprintf(stderr, "%d coverage check(s) failed — this is a COVERAGE "
                         "verdict, not a scaling verdict. Enlarge the leg.\n",
                 g_failures);
    return 1;
  }

  // ---- THE BAND, sized from realized n ----------------------------
  const double sd = std::sqrt(s91.quantileCv() * s91.quantileCv() +
                              s19.quantileCv() * s19.quantileCv());
  const double halfWidth = kSigmas * sd;
  const double bandLo = std::exp(-halfWidth);
  const double bandHi = std::exp(halfWidth);

  // The two nulls, from the realized price scales. First-order estimates
  // used ONLY for the power check below, never as a gate value.
  const double fixedNominalNull = s91.meanScale() / s19.meanScale();
  const double doubleScaledNull = s19.meanScale() / s91.meanScale();

  std::printf("\n  BAND (self-sized): +-%.1f sigma of the realized two-leg "
              "quantile SE\n",
              kSigmas);
  std::printf("      per-leg quantile CV %.4f / %.4f -> ratio SE %.4f -> band "
              "(%.4f, %.4f) around 1.0\n",
              s91.quantileCv(), s19.quantileCv(), sd, bandLo, bandHi);
  std::printf("      nulls this band must exclude: FIXED-NOMINAL %.4f, "
              "DOUBLE-SCALED %.4f\n",
              fixedNominalNull, doubleScaledNull);

  // ---- THE INSTRUMENT GATES ITS OWN POWER -------------------------
  // If the band cannot separate the defect from the claim, the gate is
  // decoration and must say so. THE REPAIR IS A LARGER LEG, NEVER A
  // TIGHTER BAND — tightening to manufacture separation would just
  // convert a power failure into a flaky gate.
  check(fixedNominalNull < bandLo,
        "UNDER-POWERED: the band's lower edge " + std::to_string(bandLo) +
            " does not exclude the FIXED-NOMINAL null " +
            std::to_string(fixedNominalNull) +
            " — this gate cannot detect the defect it exists for. Enlarge "
            "the leg (population or 1991 window); do NOT tighten the band");
  check(doubleScaledNull > bandHi,
        "UNDER-POWERED: the band's upper edge " + std::to_string(bandHi) +
            " does not exclude the DOUBLE-SCALED null " +
            std::to_string(doubleScaledNull));

  // ---- THE CLAIM ---------------------------------------------------
  const double deflated91 = quantileOf(s91.deflated, kQuantile);
  const double deflated19 = quantileOf(s19.deflated, kQuantile);
  const double deflatedRatio = deflated91 > 0.0 ? deflated19 / deflated91 : 0.0;

  // Printed discriminators, never gated. If the deflated ratio goes red,
  // these say WHICH axis moved before anyone forms a hypothesis: the
  // nominal ratio landing on the price ratio while the deflated one does
  // not is an arithmetic fault in the deflation; both moving together is
  // the amount MIX; the mean disagreeing with the quantile is the
  // unresolvable-probe share shifting. (First run: mean ratio 2.05 vs
  // quantile 1.62 — the mean rides the probe share and the heavy tail,
  // which is exactly why the withdrawn gate used the wrong statistic.)
  const double nominal91 = quantileOf(s91.nominal, kQuantile);
  const double nominal19 = quantileOf(s19.nominal, kQuantile);
  const double nominalRatio = nominal91 > 0.0 ? nominal19 / nominal91 : 0.0;
  const double meanRatio = s91.mean() > 0.0 ? s19.mean() / s91.mean() : 0.0;

  std::printf("\n  NOMINAL   Q%.0f ratio 2019/1991 %.4f   (vs realized price "
              "ratio %.4f)\n",
              kQuantile * 100.0, nominalRatio, doubleScaledNull);
  std::printf("  DEFLATED  Q%.0f ratio 2019/1991 %.4f   <- gate: in (%.4f, "
              "%.4f)\n",
              kQuantile * 100.0, deflatedRatio, bandLo, bandHi);
  std::printf("  discriminator: nominal MEAN ratio %.4f (mixture-sensitive; "
              "diverging from the quantile means the probe share moved)\n",
              meanRatio);

  check(deflatedRatio > bandLo && deflatedRatio < bandHi,
        "U-6 class F reaches the CARD rail: the deflated Q" +
            std::to_string(static_cast<int>(kQuantile * 100)) +
            " of non-lattice card fraud amounts is era-invariant (ratio " +
            std::to_string(deflatedRatio) + ", band (" +
            std::to_string(bandLo) + ", " + std::to_string(bandHi) + "))");

  if (g_failures != 0) {
    std::fprintf(stderr, "%d gate(s) failed\n", g_failures);
    return 1;
  }
  std::printf("test_card_class_f: class F reaches the card rail (deflated Q%.0f "
              "ratio %.4f in (%.4f, %.4f); nominal x%.4f vs price x%.4f; "
              "n %zu/%zu)\n",
              kQuantile * 100.0, deflatedRatio, bandLo, bandHi, nominalRatio,
              doubleScaledNull, s91.count(), s19.count());
  return 0;
}
