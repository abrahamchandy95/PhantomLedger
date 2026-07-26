#pragma once
//
// phantomledger/synth/econ/catalog.hpp
//
// The economic era of the simulated world (macro-history-v1).
// PhantomLedger simulates decades (canonically [1991-01-01,
// 2020-01-01)), and that era is historically non-stationary: prices
// roughly doubled, wages grew ≈2.5x, per-capita spending grew ≈2.9x,
// and three NBER recessions moved unemployment and consumption.
//
// The era data is EMBEDDED (era_data.hpp — constexpr tables that
// replaced the retired data/econ CSVs per the owner's minimize-repo-
// data-files directive). `macroSeries()` and `mortality()` build the
// typed views once, validate them, and return stable references.
//
// CALIBRATION YEAR (H1): the series carries the year the authority's
// calibrated dollar constants are denominated in (2019 — an owner-
// approved CHOICE pinned in era_data.hpp, validated to lie inside
// coverage). H1 consumers scale nominal amounts by
// index(year)/index(calibrationYear); the calibration year is a
// provenance fact of the calibration data and changes only together
// with the constants it denominates.
//
// CONTRACT: validated + pinned by test_econ_catalog and rendered into
// PostgreSQL by exporter::econ (test_econ_tables), but UNREAD BY
// GENERATION. Model behavior may consume these series only in the
// named macro-history H1+ rounds, after their authority rows
// (docs/fraud_model_audit.md U-4/U-5 lineage) land. The one sanctioned
// non-generation reader is the app-layer H0.6 era lock (coverage
// bounds only — so appending fully published years widens the lock
// automatically). See docs/era_data_provenance.md.
//

#include <cstddef>
#include <span>
#include <vector>

namespace PhantomLedger::synth::econ {

// One calendar year of the era. Decoded from the integer-encoded
// embedded rows (cpi_u_e3, awi_cents, ...) into natural units.
struct MacroYear {
  int year = 0;
  double cpiU = 0.0;                // CPI-U annual average, 1982-84=100
  double awiDollars = 0.0;          // SSA national Average Wage Index
  double pcePerCapitaDollars = 0.0; // nominal, exact dollars
  double unemploymentRatePct = 0.0; // U-3 ANNUAL AVERAGE (not the peak)
  int recessionMonths = 0;          // NBER months after peak..trough
  double populationThousands = 0.0; // BEA NIPA midperiod
};

// The era series: contiguous years covering at least 1990-2020
// (currently through 2024 — the last year every column is fully
// MEASURED; the AWI's ~Oct N+1 publication lag is the binding
// constraint).
class MacroSeries {
public:
  MacroSeries(std::vector<MacroYear> years, int calibrationYear)
      : years_(std::move(years)), calibrationYear_(calibrationYear) {}

  [[nodiscard]] std::span<const MacroYear> years() const noexcept {
    return years_;
  }
  [[nodiscard]] int firstYear() const noexcept { return years_.front().year; }
  [[nodiscard]] int lastYear() const noexcept { return years_.back().year; }
  [[nodiscard]] bool contains(int year) const noexcept {
    return year >= firstYear() && year <= lastYear();
  }
  // Contiguity is build-validated, so lookup is an index offset.
  [[nodiscard]] const MacroYear &at(int year) const {
    return years_.at(static_cast<std::size_t>(year - firstYear()));
  }
  // The denomination year of the authority's calibrated dollar
  // constants (build-validated to lie inside coverage).
  [[nodiscard]] int calibrationYear() const noexcept {
    return calibrationYear_;
  }

private:
  std::vector<MacroYear> years_;
  int calibrationYear_ = 0;
};

// One pivot age of the annual death-probability table.
struct MortalityPivot {
  int age = 0;
  double qxMale = 0.0;
  double qxFemale = 0.0;
};

// Annual death probability by (fractional) age and sex. Piecewise
// LOG-LINEAR interpolation in qx between listed ages (the embedded
// table is single-age, so interpolation only serves fractional ages),
// clamped outside the covered range.
class MortalityTable {
public:
  explicit MortalityTable(std::vector<MortalityPivot> pivots)
      : pivots_(std::move(pivots)) {}

  [[nodiscard]] std::span<const MortalityPivot> pivots() const noexcept {
    return pivots_;
  }
  [[nodiscard]] double qxMale(double ageYears) const noexcept {
    return interpolate(ageYears, /*male=*/true);
  }
  [[nodiscard]] double qxFemale(double ageYears) const noexcept {
    return interpolate(ageYears, /*male=*/false);
  }

private:
  [[nodiscard]] double interpolate(double ageYears, bool male) const noexcept;

  std::vector<MortalityPivot> pivots_;
};

// The era series and mortality table: built once from the embedded
// data, validated, immutable, process-wide.
[[nodiscard]] const MacroSeries &macroSeries();
[[nodiscard]] const MortalityTable &mortality();

} // namespace PhantomLedger::synth::econ
