#include "phantomledger/synth/econ/catalog.hpp"

#include "phantomledger/synth/econ/era_data.hpp"

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace PhantomLedger::synth::econ {

namespace {

[[noreturn]] void bad(const std::string &why) {
  throw std::runtime_error("embedded era data (synth/econ/era_data.hpp): " +
                           why);
}

// Structural validation of the embedded tables. Meaning gates (anchor
// ratios, recession totals, deflation year, cohort survival) live in
// test_econ_catalog; these checks keep a future data-refresh rewrite
// of era_data.hpp from shipping a malformed table.
[[nodiscard]] MacroSeries buildMacro() {
  std::vector<MacroYear> years;
  years.reserve(data::kMacroAnnual.size());
  for (const auto &row : data::kMacroAnnual) {
    MacroYear y;
    y.year = row.year;
    y.cpiU = static_cast<double>(row.cpiUE3) / 1000.0;
    y.awiDollars = static_cast<double>(row.awiCents) / 100.0;
    y.pcePerCapitaDollars = static_cast<double>(row.pceDollars);
    y.unemploymentRatePct = static_cast<double>(row.unempBp) / 100.0;
    y.recessionMonths = static_cast<int>(row.recessionMonths);
    y.populationThousands = static_cast<double>(row.populationThousands);
    years.push_back(y);
  }

  if (years.empty()) {
    bad("macro table is empty");
  }
  for (std::size_t i = 0; i < years.size(); ++i) {
    const auto &y = years[i];
    if (i > 0 && y.year != years[i - 1].year + 1) {
      bad("years are not contiguous at " + std::to_string(y.year));
    }
    if (y.cpiU <= 0.0 || y.awiDollars <= 0.0 ||
        y.pcePerCapitaDollars <= 0.0) {
      bad("non-positive index/wage/spend value in " + std::to_string(y.year));
    }
    if (y.unemploymentRatePct <= 0.0 || y.unemploymentRatePct >= 30.0) {
      bad("implausible unemployment rate in " + std::to_string(y.year));
    }
    if (y.recessionMonths < 0 || y.recessionMonths > 12) {
      bad("recession months out of [0,12] in " + std::to_string(y.year));
    }
    if (i > 0 && y.populationThousands <= years[i - 1].populationThousands) {
      bad("population not strictly increasing at " + std::to_string(y.year));
    }
  }
  if (years.front().year > 1990 || years.back().year < 2020) {
    bad("series must cover at least 1990-2020");
  }
  // The calibration year denominates the authority's dollar constants
  // (H1); a coverage rewrite must never orphan it outside the series.
  if (data::kCalibrationYear < years.front().year ||
      data::kCalibrationYear > years.back().year) {
    bad("calibration year " + std::to_string(data::kCalibrationYear) +
        " lies outside coverage");
  }

  return MacroSeries{std::move(years), data::kCalibrationYear};
}

[[nodiscard]] MortalityTable buildMortality() {
  std::vector<MortalityPivot> pivots;
  pivots.reserve(data::kMortality.size());
  for (const auto &row : data::kMortality) {
    MortalityPivot p;
    p.age = row.age;
    p.qxMale = static_cast<double>(row.qxMaleE6) / 1e6;
    p.qxFemale = static_cast<double>(row.qxFemaleE6) / 1e6;
    pivots.push_back(p);
  }

  // Log-linear interpolation needs at least two ages and positive qx.
  if (pivots.size() < 2) {
    bad("mortality table needs at least two ages");
  }
  for (std::size_t i = 0; i < pivots.size(); ++i) {
    const auto &p = pivots[i];
    if (p.age < 0 || (i > 0 && p.age <= pivots[i - 1].age)) {
      bad("mortality ages must be non-negative and strictly increasing at "
          "age " +
          std::to_string(p.age));
    }
    if (p.qxMale <= 0.0 || p.qxMale >= 1.0 || p.qxFemale <= 0.0 ||
        p.qxFemale >= 1.0) {
      bad("qx out of (0,1) at age " + std::to_string(p.age));
    }
    if (p.qxFemale > p.qxMale) {
      bad("female qx exceeds male qx at age " + std::to_string(p.age));
    }
    // US adult mortality rises with age; infant/child qx may decline.
    if (i > 0 && pivots[i - 1].age >= 30 &&
        (p.qxMale < pivots[i - 1].qxMale ||
         p.qxFemale < pivots[i - 1].qxFemale)) {
      bad("qx decreases above age 30 at age " + std::to_string(p.age));
    }
  }

  return MortalityTable{std::move(pivots)};
}

} // namespace

double MortalityTable::interpolate(double ageYears, bool male) const noexcept {
  const auto qx = [male](const MortalityPivot &p) {
    return male ? p.qxMale : p.qxFemale;
  };

  const auto &front = pivots_.front();
  const auto &back = pivots_.back();
  if (ageYears <= static_cast<double>(front.age)) {
    return qx(front);
  }
  if (ageYears >= static_cast<double>(back.age)) {
    return qx(back);
  }

  std::size_t hi = 1;
  while (static_cast<double>(pivots_[hi].age) < ageYears) {
    ++hi;
  }
  const auto &a = pivots_[hi - 1];
  const auto &b = pivots_[hi];
  const double t = (ageYears - static_cast<double>(a.age)) /
                   static_cast<double>(b.age - a.age);
  return std::exp(std::log(qx(a)) + t * (std::log(qx(b)) - std::log(qx(a))));
}

const MacroSeries &macroSeries() {
  static const MacroSeries kSeries = buildMacro();
  return kSeries;
}

const MortalityTable &mortality() {
  static const MortalityTable kTable = buildMortality();
  return kTable;
}

} // namespace PhantomLedger::synth::econ
