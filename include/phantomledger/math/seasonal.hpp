#pragma once

#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

#include <array>

namespace PhantomLedger::math::seasonal {

namespace detail {

// Index 0 is unused so month numbers map directly: table[month] for
// month in 1..12.
inline constexpr std::array<double, 13> kMonthlyRaw{
    0.0,  // sentinel
    0.88, // Jan: post-holiday trough, "dry January"
    0.94, // Feb: tax refund wave starts late
    1.04, // Mar: refund spending peak
    1.02, // Apr: late refund + final tax season
    1.00, // May: Mother's Day offsets Memorial Day drag
    0.98, // Jun: summer, Father's Day modest bump
    0.97, // Jul: mid-summer, Independence Day
    1.05, // Aug: back-to-school ramp-up
    1.02, // Sep: tail of back-to-school
    0.99, // Oct: Halloween + holiday pre-season
    1.16, // Nov: Black Friday + early holiday
    1.22, // Dec: peak holiday spending
};

[[nodiscard]] consteval std::array<double, 13> normalizeToUnitMean() {
  double sum = 0.0;
  for (std::size_t i = 1; i <= 12; ++i) {
    sum += kMonthlyRaw[i];
  }
  const double mean = sum / 12.0;
  if (mean <= 0.0) {
    return kMonthlyRaw;
  }
  const double scale = 1.0 / mean;

  std::array<double, 13> out{};
  for (std::size_t i = 1; i <= 12; ++i) {
    out[i] = kMonthlyRaw[i] * scale;
  }
  return out;
}

inline constexpr auto kMonthly = normalizeToUnitMean();

} // namespace detail

struct Config {
  bool enabled = true;
  double intensity = 1.0;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::nonNegative("intensity", intensity); });
  }
};

inline constexpr Config kDefaultConfig{};

/// Intensity scales the deviation from 1.0:
///   intensity=0 -> always 1.0 (seasonality off)
///   intensity=1 -> use kMonthly as-is
///   intensity>1 -> amplified (useful for sensitivity testing)
[[nodiscard]] constexpr double
monthlyMultiplier(int month, const Config &cfg = kDefaultConfig) noexcept {
  if (!cfg.enabled || month < 1 || month > 12) {
    return 1.0;
  }
  const double base = detail::kMonthly[static_cast<std::size_t>(month)];
  return 1.0 + (base - 1.0) * cfg.intensity;
}

[[nodiscard]] inline double
dailyMultiplier(time::TimePoint day, const Config &cfg = kDefaultConfig) {
  const auto cal = time::toCalendarDate(day);
  return monthlyMultiplier(static_cast<int>(cal.month), cfg);
}

} // namespace PhantomLedger::math::seasonal
