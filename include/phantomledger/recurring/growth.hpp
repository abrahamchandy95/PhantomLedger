#pragma once

#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/probability/distributions/lognormal.hpp"
#include "phantomledger/probability/distributions/normal.hpp"
#include "phantomledger/probability/distributions/uniform.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace PhantomLedger::recurring::growth {

namespace detail {

inline constexpr double kDaysPerYear = 365.25;

inline void requireNonNegativeYears(std::string_view field, double years) {
  namespace v = primitives::validate;

  v::finite(field, years);
  v::nonNegative(field, years);
}

inline void requireNonNegativeSigma(std::string_view field, double sigma) {
  namespace v = primitives::validate;

  v::finite(field, sigma);
  v::nonNegative(field, sigma);
}

} // namespace detail

// ---------------------------------------------------------------
// Rule values
// ---------------------------------------------------------------

struct Range {
  double min = 0.0;
  double max = 0.0;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;

    r.check([&] { v::finite("min", min); });
    r.check([&] { v::finite("max", max); });
    r.check([&] { v::nonNegative("min", min); });
    r.check([&] { v::ge("max", max, min); });
  }
};

struct GrowthDist {
  double mu = 0.0;
  double sigma = 0.0;
  double floor = -1.0;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;

    r.check([&] { v::finite("mu", mu); });
    r.check([&] { v::finite("sigma", sigma); });
    r.check([&] { v::finite("floor", floor); });
    r.check([&] { v::nonNegative("sigma", sigma); });
    r.check([&] { v::gt("floor", floor, -1.0); });
  }
};

struct CompoundRules {
  double annualInflation = 0.0;
  GrowthDist annualRaise{};

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;

    r.check([&] { v::finite("annualInflation", annualInflation); });
    r.check([&] { v::gt("annualInflation", annualInflation, -1.0); });

    annualRaise.validate(r);
  }
};

struct AnnualRaiseInput {
  std::string_view stream;
  std::string_view key;
  int year = 0;
};

// ---------------------------------------------------------------
// Date arithmetic
// ---------------------------------------------------------------

[[nodiscard]] inline int yearsToDays(double years) {
  detail::requireNonNegativeYears("years", years);
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

/// Sample tenure in days from a uniform year range.
[[nodiscard]] inline int sampleTenureDays(random::Rng &rng,
                                          const Range &range) {
  primitives::validate::require(range);

  if (range.max == range.min) {
    return std::max(1, yearsToDays(range.min));
  }

  const double years =
      probability::distributions::uniform(rng, range.min, range.max);
  return std::max(1, yearsToDays(years));
}

/// Normal draw clamped to a floor.
[[nodiscard]] inline double sampleNormalClamped(random::Rng &rng,
                                                const GrowthDist &dist) {
  primitives::validate::require(dist);

  return std::max(dist.floor,
                  probability::distributions::normal(rng, dist.mu, dist.sigma));
}

/// Lognormal multiplier centered at 1.0.
[[nodiscard]] inline double sampleLognormalMultiplier(random::Rng &rng,
                                                      double sigma) {
  detail::requireNonNegativeSigma("sigma", sigma);
  return probability::distributions::lognormal(rng, 0.0, sigma);
}

[[nodiscard]] inline double seededAnnualRaise(const random::RngFactory &factory,
                                              const GrowthDist &dist,
                                              const AnnualRaiseInput &input) {
  auto rng = factory.rng({input.stream, input.key, std::to_string(input.year)});

  return sampleNormalClamped(rng, dist);
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
                        const Range &tenure) {
  const int tenureDays = sampleTenureDays(rng, tenure);
  const int ageDays =
      static_cast<int>(rng.uniformInt(0, std::max(1, tenureDays)));

  const auto start = time::addDays(anchorDate, -ageDays);
  const auto end = time::addDays(start, tenureDays);
  return {start, end};
}

/// Sample a forward interval starting from `startDate`.
[[nodiscard]] inline Interval sampleForwardInterval(random::Rng &rng,
                                                    time::TimePoint startDate,
                                                    const Range &tenure) {
  const int tenureDays = sampleTenureDays(rng, tenure);
  return {startDate, time::addDays(startDate, tenureDays)};
}

// ---------------------------------------------------------------
// Compound growth
// ---------------------------------------------------------------

using AnnualRaiseSource = double (*)(const random::RngFactory &,
                                     const GrowthDist &,
                                     const AnnualRaiseInput &);

inline void requireGrowthFactor(double factor, std::string_view field) {
  namespace v = primitives::validate;

  v::finite(field, factor);
  v::gt(field, factor, 0.0);
}

/// Compound inflation + real raises over elapsed full years.
[[nodiscard]] inline double
compoundGrowth(const CompoundRules &rules, const random::RngFactory &factory,
               std::string_view stream, std::string_view key,
               time::TimePoint start, time::TimePoint now,
               AnnualRaiseSource raiseSource = seededAnnualRaise) {
  primitives::validate::require(rules);

  if (raiseSource == nullptr) {
    throw std::invalid_argument("compoundGrowth requires a raiseSource");
  }

  const int years = anniversariesPassed(start, now);
  if (years <= 0) {
    return 1.0;
  }

  const auto startCal = time::toCalendarDate(start);

  double growthFactor = 1.0;
  for (int i = 0; i < years; ++i) {
    const int year = startCal.year + i;

    const double realRaise = raiseSource(factory, rules.annualRaise,
                                         AnnualRaiseInput{
                                             .stream = stream,
                                             .key = key,
                                             .year = year,
                                         });

    const double factor = 1.0 + rules.annualInflation + realRaise;

    primitives::validate::finite("realRaise", realRaise);
    requireGrowthFactor(factor, "growthFactor");

    growthFactor *= factor;
  }

  return growthFactor;
}

} // namespace PhantomLedger::recurring::growth
