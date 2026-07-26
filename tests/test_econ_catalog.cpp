//
// tests/test_econ_catalog.cpp
//
// macro-history-v1 H0/H1: the EMBEDDED era reference series
// (synth::econ::macroSeries() + mortality()). No external file, no CLI
// — the series are compiled in and UNREAD by generation until the
// named H1+ model rounds. Pins:
//   * contiguous coverage: at least 1990-2020, currently through 2024
//     (the last fully MEASURED year — the AWI's ~Oct N+1 publication
//     lag is the binding constraint);
//   * the CALIBRATION YEAR (H1): 2019, inside coverage, with its two
//     scale denominators pinned exactly (CPI 255.657; AWI $54,099.99);
//   * the era's anchor ratios (CPI 2019/1991 ~ 1.88; AWI ~ 2.48;
//     per-capita PCE 2019/1990 ~ 2.9) — wages grew faster than prices;
//   * 2009 as the era's only annual CPI deflation + AWI dip;
//   * the NBER recession-month ledger (8 + 8 + 18 + 2 months);
//   * the COVID axis facts the extension added: the 2020 per-capita
//     PCE dip below 2019, 8.1% 2020 unemployment, the 2021 rebound,
//     and 2021->2022 as the era's LARGEST annual CPI jump (~8.0%);
//   * strictly increasing population (the book can grow);
//   * mortality shape: qx in (0,1), female <= male, rising with adult
//     age, log-linear interpolation between pivots, clamped ends;
//   * the arc's two cohort claims: a 22-year-old in 1991 usually
//     reaches 2020 (survival > 90%), a 65-year-old rarely reaches 94
//     (survival < 20%) — seeded-old retirees WOULD die off, which is
//     why H2/H3 age joiners young.
//

#include "phantomledger/synth/econ/catalog.hpp"

#include <cmath>
#include <cstddef>
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

// Survival from exact age `from` to exact age `to` (integer-age qx
// steps) — the same arithmetic the H3 death-date sampler will use.
double survival(double from, double to, bool male) {
  const auto &table = econ::mortality();
  double s = 1.0;
  for (double a = from; a < to; a += 1.0) {
    const double qx = male ? table.qxMale(a) : table.qxFemale(a);
    s *= (1.0 - qx);
  }
  return s;
}

} // namespace

int main() {
  const auto &m = econ::macroSeries();

  // Coverage and lookup. 2024 is the currently pinned frontier; a
  // refresh appending fully published years must keep these true.
  check(m.firstYear() <= 1990 && m.lastYear() >= 2024,
        "series covers 1990-2024, got " + std::to_string(m.firstYear()) +
            "-" + std::to_string(m.lastYear()));
  check(!m.contains(m.firstYear() - 1) && !m.contains(m.lastYear() + 1),
        "contains() rejects out-of-range years");
  check(m.contains(1991) && m.contains(2019), "canonical window years exist");

  // Calibration year (H1): the denomination year of the authority's
  // calibrated dollar constants — 2019, inside coverage, with the two
  // scale denominators H1 wiring divides by pinned exactly.
  check(m.calibrationYear() == 2019,
        "calibration year is 2019, got " +
            std::to_string(m.calibrationYear()));
  check(m.contains(m.calibrationYear()),
        "calibration year lies inside coverage");
  check(std::fabs(m.at(2019).cpiU - 255.657) < 1e-6,
        "CPI denominator at the calibration year = 255.657, got " +
            std::to_string(m.at(2019).cpiU));
  check(std::fabs(m.at(2019).awiDollars - 54099.99) < 1e-6,
        "AWI denominator at the calibration year = 54099.99, got " +
            std::to_string(m.at(2019).awiDollars));

  // Anchor ratios (DIRECTION bands, not calibration): prices ~1.88x,
  // wages ~2.48x, per-capita nominal spending ~2.9x. Wages and
  // spending grew FASTER than prices — the arc's "two series" axiom.
  const double cpiRatio = m.at(2019).cpiU / m.at(1991).cpiU;
  check(cpiRatio > 1.85 && cpiRatio < 1.90,
        "CPI 2019/1991 ~ 1.88, got " + std::to_string(cpiRatio));
  const double awiRatio = m.at(2019).awiDollars / m.at(1991).awiDollars;
  check(awiRatio > 2.40 && awiRatio < 2.55,
        "AWI 2019/1991 ~ 2.48, got " + std::to_string(awiRatio));
  const double pceRatio =
      m.at(2019).pcePerCapitaDollars / m.at(1990).pcePerCapitaDollars;
  check(pceRatio > 2.70 && pceRatio < 3.00,
        "per-capita PCE 2019/1990 ~ 2.9, got " + std::to_string(pceRatio));
  check(awiRatio > cpiRatio && pceRatio > awiRatio,
        "growth ordering: prices < wages < per-capita spending");

  // 2009: the era's only annual CPI deflation, and the AWI dipped.
  check(m.at(2009).cpiU < m.at(2008).cpiU, "2009 annual CPI deflation");
  check(m.at(2009).awiDollars < m.at(2008).awiDollars, "2009 AWI dip");
  for (const auto &y : m.years()) {
    if (y.year > m.firstYear() && y.year != 2009) {
      check(y.cpiU > m.at(y.year - 1).cpiU,
            "CPI rises every covered year except 2009, failed at " +
                std::to_string(y.year));
    }
  }

  // 2021->2022 is the era's LARGEST annual CPI jump (~8.0% — the
  // post-COVID inflation burst; every other covered yoy is smaller).
  const double burst = m.at(2022).cpiU / m.at(2021).cpiU;
  check(burst > 1.07 && burst < 1.09,
        "CPI 2022/2021 ~ 1.08, got " + std::to_string(burst));
  for (const auto &y : m.years()) {
    if (y.year > m.firstYear() && y.year != 2022) {
      check(y.cpiU / m.at(y.year - 1).cpiU < burst,
            "2021->2022 is the strict max CPI jump, exceeded at " +
                std::to_string(y.year));
    }
  }

  // NBER recession ledger: per-year months sum to the published
  // durations; canonical window [1991, 2020) holds 3 + 8 + 18 months.
  check(m.at(1990).recessionMonths == 5 && m.at(1991).recessionMonths == 3,
        "1990-91 recession = 5 + 3 months");
  check(m.at(2001).recessionMonths == 8, "2001 recession = 8 months");
  check(m.at(2008).recessionMonths == 12 && m.at(2009).recessionMonths == 6,
        "2008-09 recession = 12 + 6 months");
  check(m.at(2020).recessionMonths == 2, "2020 recession months = 2");
  check(m.at(1995).recessionMonths == 0 && m.at(2019).recessionMonths == 0,
        "expansion years carry zero recession months");
  for (int year = 2021; year <= 2024; ++year) {
    check(m.at(year).recessionMonths == 0,
          "no NBER recession months in " + std::to_string(year));
  }

  // Unemployment: Great-Recession annual averages dominate the window
  // (annual average axis — NOT monthly peaks; see
  // docs/era_data_provenance.md); COVID spiked the 2020 ANNUAL average
  // to 8.1% with a fast 2021 fall.
  check(m.at(2010).unemploymentRatePct > 9.0 &&
            m.at(2010).unemploymentRatePct > m.at(2007).unemploymentRatePct,
        "2010 unemployment above 9% and above 2007");
  check(m.at(2019).unemploymentRatePct < 4.0,
        "2019 unemployment below 4%");
  check(m.at(2020).unemploymentRatePct > 8.0,
        "2020 COVID annual-average unemployment above 8%");
  check(m.at(2021).unemploymentRatePct < m.at(2020).unemploymentRatePct,
        "2021 unemployment falls from the 2020 spike");

  // COVID spending axis: 2020 is the era's per-capita nominal PCE dip
  // (below 2019), and 2021 rebounds past 2019 — measured facts the H4
  // COVID module must eventually reproduce behaviorally.
  check(m.at(2020).pcePerCapitaDollars < m.at(2019).pcePerCapitaDollars,
        "2020 per-capita PCE dips below 2019");
  check(m.at(2021).pcePerCapitaDollars > m.at(2019).pcePerCapitaDollars,
        "2021 per-capita PCE rebounds past 2019");

  // The 2021 AWI jump (~8.9% — pandemic composition + rebound) and a
  // monotone wage index across the extension.
  check(m.at(2021).awiDollars / m.at(2020).awiDollars > 1.05,
        "2021 AWI jumps more than 5%");
  for (int year = 2020; year <= 2024; ++year) {
    check(m.at(year).awiDollars > m.at(year - 1).awiDollars,
          "AWI rises every year 2020-2024, failed at " +
              std::to_string(year));
  }

  // Population strictly increases (loader-enforced; re-pinned here as
  // the demographic-balance precondition: inflow can outrun deaths —
  // including through the 2021 COVID slowdown, the era's smallest
  // increase).
  bool popRising = true;
  for (std::size_t i = 1; i < m.years().size(); ++i) {
    if (m.years()[i].populationThousands <=
        m.years()[i - 1].populationThousands) {
      popRising = false;
    }
  }
  check(popRising, "population strictly increases across the era");

  // Mortality table shape.
  const auto &t = econ::mortality();
  check(t.pivots().size() >= 20, "mortality table has >= 20 pivot ages");
  bool qxSane = true;
  for (double age = 0.0; age <= 120.0; age += 1.0) {
    const double qm = t.qxMale(age);
    const double qf = t.qxFemale(age);
    if (!(qm > 0.0 && qm < 1.0 && qf > 0.0 && qf < 1.0) || qf > qm) {
      qxSane = false;
    }
  }
  check(qxSane, "qx in (0,1) and female <= male at every age 0-120");
  check(t.qxMale(30.0) < t.qxMale(50.0) && t.qxMale(50.0) < t.qxMale(65.0) &&
            t.qxMale(65.0) < t.qxMale(80.0),
        "male qx rises with adult age");
  check(t.qxMale(65.0) > 0.012 && t.qxMale(65.0) < 0.022,
        "male qx at 65 ~ 1.7%, got " + std::to_string(t.qxMale(65.0)));

  // Interpolation is strictly between the bracketing pivots; ends clamp.
  const double q70 = t.qxMale(70.0);
  const double q75 = t.qxMale(75.0);
  const double qMid = t.qxMale(72.5);
  check(qMid > q70 && qMid < q75,
        "interpolated qx(72.5) lies strictly between qx(70) and qx(75)");
  check(t.qxMale(130.0) == t.pivots().back().qxMale,
        "ages past the last pivot clamp to it");

  // The arc's cohort claims (macro-history-v1, MORTALITY anchor): a
  // 22-year-old in 1991 usually survives the 29-year window; a
  // 65-year-old rarely reaches 94. Seeded-old retirees WOULD die off —
  // H2 ages retirees in and H3 pairs deaths with young joiner inflow.
  const double young = survival(22.0, 51.0, /*male=*/true);
  check(young > 0.90, "male survival 22->51 above 90%, got " +
                          std::to_string(young));
  const double old_ = survival(65.0, 94.0, /*male=*/true);
  check(old_ < 0.20, "male survival 65->94 below 20%, got " +
                         std::to_string(old_));

  if (g_failures != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("test_econ_catalog: all checks passed (%zu years, %zu "
              "mortality pivots)\n",
              m.years().size(), t.pivots().size());
  return 0;
}
