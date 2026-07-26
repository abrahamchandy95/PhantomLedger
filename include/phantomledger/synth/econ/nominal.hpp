#pragma once
//
// phantomledger/synth/econ/nominal.hpp
//
// THE H1 NOMINAL SCALE (macro-history-v1): the two deterministic
// level multipliers that turn the authority's calibration-year
// dollar constants into era-correct nominal amounts.
//
//   priceScale(year) = CPI(year)  / CPI(calibrationYear)   — prices,
//     consumption, rents, premiums, fees, fraud amounts (except the
//     statutory class), debt principals at ORIGINATION.
//   wageScale(year)  = AWI(year)  / AWI(calibrationYear)   — labor
//     income: salaries, freelancer/business revenue, SSA benefit
//     levels (declared CHOICE).
//
// THE H4 REAL CONSUMPTION LEVEL (authority U-9, contract
// docs/h4_macro_modulation.md):
//
//   pceScale(year)     = PCE/capita(year) / PCE/capita(calibrationYear)
//     — the nominal per-capita personal-consumption index (BEA
//     A794RC, embedded).
//   realPceLevel(year) = pceScale(year) / priceScale(year)
//     — the measured REAL per-capita consumption path (~0.67 at 1991,
//     exactly 1.0 at 2019), carrying the 1990-91/2001/2008-09
//     recession dips and the 2020 collapse + 2021 rebound at annual
//     resolution. THE CHANNEL (owner decision 2026-07-26): this level
//     modulates the discretionary session's transaction COUNT axis
//     only — ticket amounts stay priceScale-realized, so the U-6
//     denomination law is untouched and the fraud budget follows the
//     candidate count automatically. CONSUMERS ARRIVE ONLY in the
//     named H4 step-2 wiring round; until it lands these two are
//     pinned by test_econ_scale and read by nothing in generation.
//
// All four are LEVEL-ANCHORED: at the calibration year (2019, pinned
// in era_data.hpp) every scale is EXACTLY 1.0, so today's calibrated
// magnitudes and volumes are what a 2019 window produces; a 1991
// window opens at ≈0.40x wages, ≈0.53x prices, ≈0.67x real volume.
//
// FREEZE-AND-DECLARE (owner's durability criterion): outside coverage
// the scale CLAMPS to the nearest covered year — the measured frontier
// always lags now-time, and beyond it the economy holds the last
// measured level. `scaleFrozen(year)` reports the clamp so the app
// layer can print the declared notice (step-2b wiring); the scales
// themselves NEVER extrapolate and NEVER read the wall clock.
//
// PURITY / LAW COMPLIANCE: pure functions of (embedded series, year) —
// no RNG, no lanes, no draws moved; wiring multiplies AFTER a site's
// existing draw (H1) or as a per-day-frame level factor (H4), so
// replay and the windowed==monolith oracle stay exact (both engines
// call the same functions).
//

#include "phantomledger/synth/econ/catalog.hpp"

#include <algorithm>

namespace PhantomLedger::synth::econ {

// The year the scale actually reads for `year`: clamped to coverage.
[[nodiscard]] inline int scaleYear(int year) {
  const auto &m = macroSeries();
  return std::clamp(year, m.firstYear(), m.lastYear());
}

// True when `year` lies outside the measured coverage — the scale is
// frozen at the nearest covered year's level. The consumer that first
// realizes a frozen year prints the declared notice (step-2b).
[[nodiscard]] inline bool scaleFrozen(int year) {
  const auto &m = macroSeries();
  return year < m.firstYear() || year > m.lastYear();
}

// Price-level multiplier relative to the calibration year (CPI-U
// annual average axis). Exactly 1.0 at the calibration year.
[[nodiscard]] inline double priceScale(int year) {
  const auto &m = macroSeries();
  return m.at(scaleYear(year)).cpiU / m.at(m.calibrationYear()).cpiU;
}

// Wage-level multiplier relative to the calibration year (SSA AWI
// axis). Exactly 1.0 at the calibration year.
[[nodiscard]] inline double wageScale(int year) {
  const auto &m = macroSeries();
  return m.at(scaleYear(year)).awiDollars /
         m.at(m.calibrationYear()).awiDollars;
}

// Nominal per-capita consumption multiplier relative to the
// calibration year (BEA A794RC axis). Exactly 1.0 at the calibration
// year. H4; unread by generation until the step-2 wiring.
[[nodiscard]] inline double pceScale(int year) {
  const auto &m = macroSeries();
  return m.at(scaleYear(year)).pcePerCapitaDollars /
         m.at(m.calibrationYear()).pcePerCapitaDollars;
}

// The REAL per-capita consumption level: the nominal consumption
// index deflated by the price index. Exactly 1.0 at the calibration
// year (both factors are). H4's count-axis modulation factor; unread
// by generation until the step-2 wiring.
[[nodiscard]] inline double realPceLevel(int year) {
  return pceScale(year) / priceScale(year);
}

} // namespace PhantomLedger::synth::econ
