//
// tests/test_econ_scale.cpp
//
// macro-history-v1 H1 step 2a + H4 step 1: the nominal-scale and
// real-consumption-level primitives (synth/econ/nominal.hpp) — the
// deterministic level multipliers the named wiring rounds multiply
// into dollar realization (H1: priceScale/wageScale, WIRED at step
// 2b) and into the session's count axis (H4: pceScale/realPceLevel,
// UNWIRED until the H4 step-2 round). This test pins the MEANING so
// each wiring round only has to pin the wiring. Pins:
//   * exact 1.0 at the calibration year (2019) on ALL axes;
//   * level anchoring backward: 1991 prices ≈ 0.53x, wages ≈ 0.40x —
//     and wageScale(1991) < priceScale(1991) (wages grew FASTER, so
//     the backward wage scale is smaller); nominal consumption
//     ≈ 0.36x and REAL consumption ≈ 0.67x (authority U-9);
//   * level anchoring forward: 2024 wages ≈ 1.29x, prices ≈ 1.23x —
//     wage scale larger looking forward, both above 1; the 2024 real
//     consumption level back ABOVE calibration;
//   * the measured real-consumption dips: 1991 below 1990, 2009
//     below 2008 (the GFC), 2020 below 2019 (COVID), and the 2021
//     rebound — encoded by the series, inherited by the level;
//   * consistency with the raw series (scale ratios == index ratios);
//   * monotone price scale except the 2009 deflation;
//   * FREEZE-AND-DECLARE: outside coverage every scale clamps to the
//     nearest covered year and scaleFrozen() reports it — never
//     extrapolated, never wall-clock.
//

#include "phantomledger/synth/econ/nominal.hpp"

#include <cmath>
#include <cstdio>
#include <string>

namespace econ = ::PhantomLedger::synth::econ;

namespace {

int g_failures = 0;

void check(bool cond, const std::string &what) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

} // namespace

int main() {
  const auto &m = econ::macroSeries();
  const int cal = m.calibrationYear();

  // Exact identity at the calibration year: the ratio of a value with
  // itself is exactly 1.0 in IEEE double arithmetic.
  check(econ::priceScale(cal) == 1.0, "priceScale(calibration year) == 1.0");
  check(econ::wageScale(cal) == 1.0, "wageScale(calibration year) == 1.0");
  check(econ::pceScale(cal) == 1.0, "pceScale(calibration year) == 1.0");
  check(econ::realPceLevel(cal) == 1.0,
        "realPceLevel(calibration year) == 1.0");
  check(!econ::scaleFrozen(cal), "calibration year is not frozen");

  // Backward anchoring: a 1991 window opens at ~0.53x prices, ~0.40x
  // wages (DIRECTION bands around the audited index ratios).
  const double p1991 = econ::priceScale(1991);
  check(p1991 > 0.52 && p1991 < 0.55,
        "priceScale(1991) ~ 0.53, got " + std::to_string(p1991));
  const double w1991 = econ::wageScale(1991);
  check(w1991 > 0.39 && w1991 < 0.42,
        "wageScale(1991) ~ 0.40, got " + std::to_string(w1991));
  check(w1991 < p1991,
        "backward wage scale below price scale (wages grew faster)");

  // H4: nominal consumption ~0.36x, REAL consumption ~0.67x at 1991 —
  // per-capita spending grew FASTER than prices (the count-axis
  // modulation the step-2 wiring realizes; authority U-9).
  const double c1991 = econ::pceScale(1991);
  check(c1991 > 0.34 && c1991 < 0.38,
        "pceScale(1991) ~ 0.36, got " + std::to_string(c1991));
  const double r1991 = econ::realPceLevel(1991);
  check(r1991 > 0.63 && r1991 < 0.71,
        "realPceLevel(1991) ~ 0.67, got " + std::to_string(r1991));
  check(c1991 < p1991,
        "backward consumption index below price index (real growth)");

  // Forward anchoring: 2024 wages outgrow 2024 prices, both above 1;
  // the real consumption level is back above calibration.
  const double p2024 = econ::priceScale(2024);
  const double w2024 = econ::wageScale(2024);
  check(p2024 > 1.20 && p2024 < 1.25,
        "priceScale(2024) ~ 1.23, got " + std::to_string(p2024));
  check(w2024 > 1.26 && w2024 < 1.32,
        "wageScale(2024) ~ 1.29, got " + std::to_string(w2024));
  check(w2024 > p2024, "forward wage scale above price scale");
  check(econ::realPceLevel(2024) > 1.0,
        "realPceLevel(2024) above calibration (recovered real growth)");

  // The measured real-consumption path: level dips in 1991 (the
  // 1990-91 recession), 2009 (the GFC), 2020 (COVID), and the 2021
  // rebound. (2001 deliberately unpinned: that recession slowed
  // growth without a per-capita consumption dip — the series says
  // so, and the model inherits it.)
  check(econ::realPceLevel(1991) < econ::realPceLevel(1990),
        "realPceLevel dips in 1991 (the 1990-91 recession)");
  check(econ::realPceLevel(2009) < econ::realPceLevel(2008),
        "realPceLevel dips in 2009 (the GFC)");
  check(econ::realPceLevel(2020) < econ::realPceLevel(2019),
        "realPceLevel dips in 2020 (COVID)");
  check(econ::realPceLevel(2021) > econ::realPceLevel(2020),
        "realPceLevel rebounds in 2021");

  // Consistency with the raw series: scale ratios ARE index ratios.
  {
    const double fromScale = econ::priceScale(1991) / econ::priceScale(1990);
    const double fromSeries = m.at(1991).cpiU / m.at(1990).cpiU;
    check(std::fabs(fromScale - fromSeries) < 1e-12,
          "price scale ratio == CPI ratio (1990->1991)");
    const double wFromScale = econ::wageScale(2021) / econ::wageScale(2020);
    const double wFromSeries = m.at(2021).awiDollars / m.at(2020).awiDollars;
    check(std::fabs(wFromScale - wFromSeries) < 1e-12,
          "wage scale ratio == AWI ratio (2020->2021)");
    const double cFromScale = econ::pceScale(1991) / econ::pceScale(1990);
    const double cFromSeries =
        m.at(1991).pcePerCapitaDollars / m.at(1990).pcePerCapitaDollars;
    check(std::fabs(cFromScale - cFromSeries) < 1e-12,
          "consumption scale ratio == PCE ratio (1990->1991)");
  }

  // Price scale rises every covered year except the 2009 deflation.
  for (const auto &y : m.years()) {
    if (y.year == m.firstYear()) {
      continue;
    }
    const double prev = econ::priceScale(y.year - 1);
    const double cur = econ::priceScale(y.year);
    if (y.year == 2009) {
      check(cur < prev, "priceScale falls in 2009 (the only deflation)");
    } else {
      check(cur > prev, "priceScale rises in " + std::to_string(y.year));
    }
  }

  // FREEZE-AND-DECLARE: outside coverage every scale clamps to the
  // nearest covered year; scaleFrozen reports exactly those years.
  check(econ::scaleFrozen(m.lastYear() + 1) &&
            econ::scaleFrozen(m.firstYear() - 1),
        "years outside coverage are frozen");
  check(!econ::scaleFrozen(m.firstYear()) && !econ::scaleFrozen(m.lastYear()),
        "coverage edge years are not frozen");
  check(econ::priceScale(m.lastYear() + 10) == econ::priceScale(m.lastYear()),
        "beyond-frontier price scale freezes at the last measured year");
  check(econ::wageScale(m.lastYear() + 10) == econ::wageScale(m.lastYear()),
        "beyond-frontier wage scale freezes at the last measured year");
  check(econ::pceScale(m.lastYear() + 10) == econ::pceScale(m.lastYear()),
        "beyond-frontier consumption scale freezes at the last measured "
        "year");
  check(econ::realPceLevel(m.lastYear() + 10) ==
            econ::realPceLevel(m.lastYear()),
        "beyond-frontier real level freezes at the last measured year");
  check(econ::priceScale(m.firstYear() - 10) ==
            econ::priceScale(m.firstYear()),
        "pre-coverage price scale freezes at the first measured year");
  check(econ::scaleYear(m.lastYear() + 10) == m.lastYear() &&
            econ::scaleYear(m.firstYear() - 10) == m.firstYear() &&
            econ::scaleYear(cal) == cal,
        "scaleYear clamps to coverage and is identity inside it");

  if (g_failures != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("test_econ_scale: all checks passed (calibration year %d; "
              "coverage %d-%d; realPceLevel(1991) %.3f)\n",
              cal, m.firstYear(), m.lastYear(), econ::realPceLevel(1991));
  return 0;
}
