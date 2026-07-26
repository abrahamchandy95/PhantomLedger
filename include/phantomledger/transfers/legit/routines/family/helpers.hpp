#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/constants.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/synth/econ/nominal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>

namespace PhantomLedger::transfers::legit::routines::family::helpers {

[[nodiscard]] inline double sanitizeAmount(double amount,
                                           double floor) noexcept {
  const double rounded = primitives::utils::roundMoney(amount);
  return rounded >= floor ? rounded : 0.0;
}

/// H1 step 2b (class P): realize a calibration-year family amount at
/// the event date's CPI level. Applied AFTER sanitizeAmount, so the
/// behavioral floor scales with the amounts it screens (authority
/// U-6) and a sanitized 0 stays 0.
[[nodiscard]] inline double nominalAt(double amount,
                                      std::int64_t epochSec) noexcept {
  if (amount == 0.0) {
    return 0.0;
  }
  const auto year = ::PhantomLedger::time::toCalendarDate(
                        ::PhantomLedger::time::fromEpochSeconds(epochSec))
                        .year;
  return primitives::utils::roundMoney(
      amount * ::PhantomLedger::synth::econ::priceScale(year));
}

[[nodiscard]] inline std::int64_t
randomTimestampInMonth(::PhantomLedger::time::TimePoint monthStart,
                       int monthDays, random::Rng &rng) {
  if (monthDays <= 0) {
    return ::PhantomLedger::time::toEpochSeconds(monthStart);
  }

  const auto dayOffset =
      static_cast<int>(rng.uniformInt(0, static_cast<std::int64_t>(monthDays)));
  const auto secOffset =
      rng.uniformInt(0, ::PhantomLedger::time::kSecondsPerDay);

  const auto base = ::PhantomLedger::time::toEpochSeconds(
      ::PhantomLedger::time::addDays(monthStart, dayOffset));
  return base + secOffset;
}

struct PostingShape {
  std::int64_t dayMaxExcl = 0;
  std::int64_t hourMin = 0;
  std::int64_t hourMaxExcl = 24;
};

[[nodiscard]] inline std::int64_t
sampleMonthlyPostingTs(random::Rng &rng,
                       ::PhantomLedger::time::TimePoint monthStart,
                       const PostingShape &shape) {
  const auto day = rng.uniformInt(0, shape.dayMaxExcl);
  const auto hour = rng.uniformInt(shape.hourMin, shape.hourMaxExcl);
  const auto minute = rng.uniformInt(0, 60);

  const auto base = ::PhantomLedger::time::toEpochSeconds(
      ::PhantomLedger::time::addDays(monthStart, static_cast<int>(day)));
  return base + ::PhantomLedger::time::secondsInDay(hour, minute);
}

[[nodiscard]] inline double pareto(random::Rng &rng, double xm,
                                   double alpha) noexcept {
  const double u = 1.0 - rng.nextDouble();
  const double sample = xm / std::pow(u, 1.0 / alpha);
  return std::max(xm, sample);
}

[[nodiscard]] inline double
capacityFor(entity::PersonId person,
            std::span<const double> multipliers) noexcept {
  if (multipliers.empty() || !entity::valid(person) ||
      person > multipliers.size()) {
    return 1.0;
  }

  return multipliers[person - 1];
}

[[nodiscard]] inline entity::PersonId
weightedPickPerson(std::span<const entity::PersonId> pool,
                   std::span<const double> multipliers, random::Rng &rng) {
  if (pool.empty()) {
    return entity::invalidPerson;
  }

  if (pool.size() == 1) {
    return pool.front();
  }

  double total = 0.0;
  for (const auto p : pool) {
    total += capacityFor(p, multipliers);
  }

  if (total <= 0.0) {
    const auto idx = static_cast<std::size_t>(
        rng.uniformInt(0, static_cast<std::int64_t>(pool.size())));
    return pool[idx];
  }

  const auto u = rng.nextDouble() * total;
  double accum = 0.0;

  for (const auto p : pool) {
    accum += capacityFor(p, multipliers);
    if (u < accum) {
      return p;
    }
  }

  return pool.back();
}

} // namespace PhantomLedger::transfers::legit::routines::family::helpers
