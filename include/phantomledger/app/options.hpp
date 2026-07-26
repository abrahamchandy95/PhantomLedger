#pragma once

#include "phantomledger/primitives/time/calendar.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace PhantomLedger::app {

// Production default when PL_PG is absent. Keep this named so the runtime,
// usage text, and regression test cannot silently drift to libpq's implicit
// OS-user database.
inline constexpr char kDefaultPgConninfo[] = "dbname=phantomledger";

[[nodiscard]] inline std::string
pgConninfoWithOverride(std::string_view fallback, const char *overrideValue) {
  if (overrideValue == nullptr || *overrideValue == '\0') {
    return std::string{fallback};
  }
  return std::string{overrideValue};
}

enum class UseCase : std::uint8_t {
  standard = 0,
  muleMl = 1,
  aml = 2,
  amlTxnEdges = 3,
  cardFraud = 4,
};

[[nodiscard]] constexpr std::string_view name(UseCase uc) noexcept {
  switch (uc) {
  case UseCase::standard:
    return "standard";
  case UseCase::muleMl:
    return "mule-ml";
  case UseCase::aml:
    return "aml";
  case UseCase::amlTxnEdges:
    return "aml-txn-edges";
  case UseCase::cardFraud:
    return "card-fraud";
  }
  return "<unknown>";
}

[[nodiscard]] constexpr std::optional<UseCase>
parseUseCase(std::string_view s) noexcept {
  if (s == "standard") {
    return UseCase::standard;
  }
  if (s == "mule-ml") {
    return UseCase::muleMl;
  }
  if (s == "aml") {
    return UseCase::aml;
  }
  if (s == "aml-txn-edges") {
    return UseCase::amlTxnEdges;
  }
  if (s == "card-fraud") {
    return UseCase::cardFraud;
  }
  return std::nullopt;
}

inline constexpr std::array<UseCase, 5> kAllUseCases{
    UseCase::standard,    UseCase::muleMl,    UseCase::aml,
    UseCase::amlTxnEdges, UseCase::cardFraud,
};

struct RunOptions {
  UseCase usecase = UseCase::standard;
  std::int64_t days = 365;
  std::int32_t population = 70'000;
  std::uint64_t seed = 0xDEADBEEFULL;

  std::string pgConninfo = kDefaultPgConninfo;
  ::PhantomLedger::time::CalendarDate startDate{2025, 1, 1};
};

// ---------------------------------------------------------------
// Era lock (macro-history-v1 H0.6, owner directive #3)
//
// The card-fraud use case models the frozen measured economic era
// (its reference artifact spans 1991-2020, and its realism anchors
// are the era history EMBEDDED in synth/econ/era_data.hpp), so its
// requested window must lie INSIDE the era-series coverage. The
// bounds come from the pinned data via synth::econ::macroSeries() —
// 1990 through the measured frontier (2024 since the H1 coverage
// extension; appending a fully published year widens the lock with
// no code change). The predicate lives here, pure and
// coverage-parameterized, so test_app_options can exercise the
// boundary cases in-process (cli::parse exits on failure).
// ---------------------------------------------------------------

// Last simulated calendar day of the half-open window
// [start, start + days). The span is clamped at ~1000 years to keep
// the day arithmetic safe for absurd --days values; any such window
// is outside every pinned era regardless.
[[nodiscard]] inline ::PhantomLedger::time::CalendarDate
lastSimulatedDay(const RunOptions &opts) {
  const std::int64_t span = std::min<std::int64_t>(
      std::max<std::int64_t>(opts.days, 1) - 1, 400'000);
  const auto start = ::PhantomLedger::time::makeTime(opts.startDate);
  return ::PhantomLedger::time::toCalendarDate(
      ::PhantomLedger::time::addDays(start, static_cast<int>(span)));
}

// True when every simulated day falls inside calendar years
// [eraFirstYear, eraLastYear] (era coverage is whole years).
[[nodiscard]] inline bool
windowInsideEra(const RunOptions &opts, int eraFirstYear, int eraLastYear) {
  if (opts.days <= 0 || opts.days - 1 > 400'000) {
    return false;
  }
  return opts.startDate.year >= eraFirstYear &&
         lastSimulatedDay(opts).year <= eraLastYear;
}

// ---------------------------------------------------------------
// Frozen-era declared notice (macro-history-v1 H1 step 2b)
//
// FREEZE-AND-DECLARE (the owner's durability criterion): outside the
// measured coverage the H1 nominal scales clamp to the nearest
// covered year's level — never extrapolated, never wall-clock. The
// engine must OWN that fact out loud: main prints this notice ONCE to
// stderr for any window touching frozen years (e.g. the 2025 default
// start until the 2025 data row lands). Pure and
// coverage-parameterized, like windowInsideEra above, so
// test_app_options pins the wording and the boundary cases; nullopt
// means the whole window is measured.
// ---------------------------------------------------------------
[[nodiscard]] inline std::optional<std::string>
frozenEraNotice(const RunOptions &opts, int eraFirstYear, int eraLastYear) {
  if (opts.days <= 0) {
    return std::nullopt;
  }
  const int firstYear = opts.startDate.year;
  const int lastYear = lastSimulatedDay(opts).year;
  if (firstYear >= eraFirstYear && lastYear <= eraLastYear) {
    return std::nullopt;
  }

  std::string out = "note: window years ";
  out += std::to_string(firstYear);
  out += "-";
  out += std::to_string(lastYear);
  out += " extend beyond measured era coverage ";
  out += std::to_string(eraFirstYear);
  out += "-";
  out += std::to_string(eraLastYear);
  out += "; nominal dollar scales FREEZE at the ";
  out += std::to_string(lastYear > eraLastYear ? eraLastYear : eraFirstYear);
  out += " level for the uncovered years (declared freeze-and-declare, "
         "never extrapolated)";
  return out;
}

} // namespace PhantomLedger::app
