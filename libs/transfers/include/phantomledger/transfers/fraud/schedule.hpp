#pragma once

#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace PhantomLedger::transfers::fraud {

[[nodiscard]] inline std::int64_t
calculateIllicitBudget(double targetRatio, std::int64_t legitCount) noexcept {
  if (targetRatio <= 0.0 || legitCount <= 0) {
    return 0;
  }

  const double denom = std::max(1e-12, 1.0 - targetRatio);
  const double raw = (targetRatio * static_cast<double>(legitCount)) / denom;
  const auto rounded = static_cast<std::int64_t>(std::llround(raw));
  return std::max<std::int64_t>(0, rounded);
}

struct BurstWindow {
  time::TimePoint baseDate{};
  std::int32_t durationDays = 0;
};

struct BurstShape {
  std::int32_t tailPaddingDays = 0;
  std::int32_t minDays = 0;
  std::int32_t maxDays = 0;

  [[nodiscard]] std::int32_t
  maxStartOffset(std::int32_t totalDays) const noexcept {
    return std::max<std::int32_t>(1, totalDays - tailPaddingDays);
  }
};

[[nodiscard]] inline BurstWindow sampleBurstWindow(random::Rng &rng,
                                                   time::TimePoint startDate,
                                                   std::int32_t totalDays,
                                                   const BurstShape &shape) {
  const auto offsetDays = static_cast<std::int32_t>(rng.uniformInt(
      0, static_cast<std::int64_t>(shape.maxStartOffset(totalDays)) + 1));

  const auto burstDays = static_cast<std::int32_t>(rng.uniformInt(
      shape.minDays, static_cast<std::int64_t>(shape.maxDays) + 1));

  return BurstWindow{
      .baseDate = startDate + time::Days{offsetDays},
      .durationDays = burstDays,
  };
}

struct HourRange {
  std::int32_t min = 0;
  std::int32_t max = 23;

  [[nodiscard]] std::int32_t pick(random::Rng &rng) const {
    return static_cast<std::int32_t>(
        rng.uniformInt(min, static_cast<std::int64_t>(max) + 1));
  }
};

[[nodiscard]] inline time::TimePoint sampleTimestamp(random::Rng &rng,
                                                     time::TimePoint baseDate,
                                                     std::int32_t maxDaysOffset,
                                                     HourRange hours) {
  const auto dayBound = std::max<std::int32_t>(1, maxDaysOffset);

  const auto dayOffset = static_cast<std::int32_t>(
      rng.uniformInt(0, static_cast<std::int64_t>(dayBound) + 1));

  const auto hour = hours.pick(rng);

  const auto minute = static_cast<std::int32_t>(rng.uniformInt(0, 61));

  return baseDate + time::Days{dayOffset} + time::Hours{hour} +
         time::Minutes{minute};
}

} // namespace PhantomLedger::transfers::fraud
