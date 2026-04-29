#pragma once

#include "phantomledger/entropy/random/rng.hpp"
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

[[nodiscard]] inline BurstWindow
sampleBurstWindow(random::Rng &rng, time::TimePoint startDate,
                  std::int32_t totalDays, std::int32_t tailPaddingDays,
                  std::int32_t minBurstDays, std::int32_t maxBurstDays) {
  const auto maxStartOffset =
      std::max<std::int32_t>(1, totalDays - tailPaddingDays);

  const auto offsetDays = static_cast<std::int32_t>(
      rng.uniformInt(0, static_cast<std::int64_t>(maxStartOffset) + 1));

  const auto burstDays = static_cast<std::int32_t>(rng.uniformInt(
      minBurstDays, static_cast<std::int64_t>(maxBurstDays) + 1));

  return BurstWindow{
      .baseDate = startDate + time::Days{offsetDays},
      .durationDays = burstDays,
  };
}

[[nodiscard]] inline time::TimePoint sampleTimestamp(random::Rng &rng,
                                                     time::TimePoint baseDate,
                                                     std::int32_t maxDaysOffset,
                                                     std::int32_t minHour,
                                                     std::int32_t maxHour) {
  const auto dayBound = std::max<std::int32_t>(1, maxDaysOffset);

  const auto dayOffset = static_cast<std::int32_t>(
      rng.uniformInt(0, static_cast<std::int64_t>(dayBound) + 1));

  const auto hour = static_cast<std::int32_t>(
      rng.uniformInt(minHour, static_cast<std::int64_t>(maxHour) + 1));

  const auto minute = static_cast<std::int32_t>(rng.uniformInt(0, 61));

  return baseDate + time::Days{dayOffset} + time::Hours{hour} +
         time::Minutes{minute};
}

} // namespace PhantomLedger::transfers::fraud
