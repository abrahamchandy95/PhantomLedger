#pragma once
/*
 * Growth and lifecycle sampling helpers.
 *
 * Shared utilities for the employment and lease state machines:
 * compound growth over anniversary boundaries, tenure sampling,
 * and counterparty selection.
 */

#include "phantomledger/math/sampling.hpp"
#include "phantomledger/random/factory.hpp"
#include "phantomledger/random/rng.hpp"
#include "phantomledger/time/calendar.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string_view>

namespace PhantomLedger::recurring::growth {

namespace detail {

inline constexpr double kDaysPerYear = 365.25;

} // namespace detail

// ---------------------------------------------------------------
// Date arithmetic
// ---------------------------------------------------------------

[[nodiscard]] inline int yearsToDays(double years) {
  return static_cast<int>(std::round(years * detail::kDaysPerYear));
}

/// Count full 12-month anniversaries between start and now.
[[nodiscard]] inline int anniversariesPassed(time::TimePoint start,
                                             time::TimePoint now) {
  const auto startCal = time::toCalendarDate(start);
  const auto nowCal = time::toCalendarDate(now);

  int years = nowCal.year - startCal.year;
  if (nowCal.month < startCal.month ||
      (nowCal.month == startCal.month && nowCal.day < startCal.day)) {
    --years;
  }

  return std::max(0, years);
}

// ---------------------------------------------------------------
// Sampling
// ---------------------------------------------------------------

/// Sample tenure in days from a uniform [yearsMin, yearsMax] range.
[[nodiscard]] inline int sampleTenureDays(random::Rng &rng, double yearsMin,
                                          double yearsMax) {
  if (yearsMax <= yearsMin) {
    return std::max(1, yearsToDays(yearsMin));
  }

  const double years = math::uniform(rng, yearsMin, yearsMax);
  return std::max(1, yearsToDays(years));
}

/// Normal draw clamped to a floor.
[[nodiscard]] inline double sampleNormalClamped(random::Rng &rng, double mu,
                                                double sigma, double floor) {
  return std::max(floor, math::normal(rng, mu, sigma));
}

/// Lognormal multiplier centered at 1.0.
[[nodiscard]] inline double sampleLognormalMultiplier(random::Rng &rng,
                                                      double sigma) {
  return math::lognormal(rng, 0.0, sigma);
}

// ---------------------------------------------------------------
// Counterparty selection
// ---------------------------------------------------------------

/// Pick one item uniformly from a span.
template <typename T>
[[nodiscard]] inline const T &pickOne(random::Rng &rng,
                                      std::span<const T> items) {
  if (items.empty()) {
    throw std::invalid_argument("pickOne requires non-empty items");
  }

  return items[rng.choiceIndex(items.size())];
}

/// Pick one item different from `current`. Falls back to a uniform
/// pick if `current` is not present. If the pool has only one item,
/// returns that item.
template <typename T>
[[nodiscard]] inline const T &
pickDifferent(random::Rng &rng, std::span<const T> items, const T &current) {
  if (items.empty()) {
    throw std::invalid_argument("pickDifferent requires non-empty items");
  }

  if (items.size() == 1) {
    return items[0];
  }

  const auto it = std::find(items.begin(), items.end(), current);
  if (it == items.end()) {
    return items[rng.choiceIndex(items.size())];
  }

  const auto currentIndex =
      static_cast<std::size_t>(std::distance(items.begin(), it));

  auto pickIndex = rng.choiceIndex(items.size() - 1);
  if (pickIndex >= currentIndex) {
    ++pickIndex;
  }

  return items[pickIndex];
}

// ---------------------------------------------------------------
// Interval sampling
// ---------------------------------------------------------------

struct Interval {
  time::TimePoint start;
  time::TimePoint end;
};

/// Sample a backdated interval: the anchor falls somewhere within
/// the tenure, so the start is before anchorDate.
[[nodiscard]] inline Interval
sampleBackdatedInterval(random::Rng &rng, time::TimePoint anchorDate,
                        double yearsMin, double yearsMax) {
  const int tenureDays = sampleTenureDays(rng, yearsMin, yearsMax);
  const int ageDays =
      static_cast<int>(rng.uniformInt(0, std::max(1, tenureDays)));

  const auto start = time::addDays(anchorDate, -ageDays);
  const auto end = time::addDays(start, tenureDays);
  return {start, end};
}

/// Sample a forward interval starting from `startDate`.
[[nodiscard]] inline Interval sampleForwardInterval(random::Rng &rng,
                                                    time::TimePoint startDate,
                                                    double yearsMin,
                                                    double yearsMax) {
  const int tenureDays = sampleTenureDays(rng, yearsMin, yearsMax);
  return {startDate, time::addDays(startDate, tenureDays)};
}

// ---------------------------------------------------------------
// Compound growth
// ---------------------------------------------------------------

/// Callback type for per-year real raise sampling.
/// Signature: double(const random::RngFactory&, std::string_view key, int year)
using AnnualRaiseSource = double (*)(const random::RngFactory &,
                                     std::string_view, int);

/// Compound inflation + real raises over elapsed full years.
[[nodiscard]] inline double
compoundGrowth(double inflation, const random::RngFactory &rngFactory,
               std::string_view key, time::TimePoint start, time::TimePoint now,
               AnnualRaiseSource raiseSource) {
  if (raiseSource == nullptr) {
    throw std::invalid_argument("compoundGrowth requires a raiseSource");
  }

  const int years = anniversariesPassed(start, now);
  if (years <= 0) {
    return 1.0;
  }

  const auto startCal = time::toCalendarDate(start);
  const double base = 1.0 + inflation;

  double growth = 1.0;
  for (int i = 0; i < years; ++i) {
    const int year = startCal.year + i;
    const double realRaise = raiseSource(rngFactory, key, year);
    growth *= (base + realRaise);
  }

  return growth;
}

} // namespace PhantomLedger::recurring::growth
