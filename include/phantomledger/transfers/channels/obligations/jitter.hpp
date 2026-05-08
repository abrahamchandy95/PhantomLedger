#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

#include <cstdint>

namespace PhantomLedger::transfers::obligations::jitter {

/// Light jitter for non-installment events
[[nodiscard]] inline time::TimePoint basicJitter(random::Rng &rng,
                                                 time::TimePoint ts) noexcept {
  const auto days = rng.uniformInt(0, 3);
  const auto hours = rng.uniformInt(0, 12);
  const auto minutes = rng.uniformInt(0, 60);
  return ts + time::Days{days} + time::Hours{hours} + time::Minutes{minutes};
}

/// Timestamp for a realized installment payment.
[[nodiscard]] inline time::TimePoint
installmentTimestamp(random::Rng &rng, time::TimePoint dueTs, double lateP,
                     std::int32_t lateDaysMin, std::int32_t lateDaysMax,
                     bool forceLate) {
  const bool beLate = forceLate || rng.coin(lateP);

  if (beLate) {
    int delayDays = 0;
    if (lateDaysMax > 0) {
      delayDays =
          static_cast<int>(rng.uniformInt(lateDaysMin, lateDaysMax + 1));
    }
    const auto hour = rng.uniformInt(8, 22);
    const auto minute = rng.uniformInt(0, 60);
    return dueTs + time::Days{delayDays} + time::Hours{hour} +
           time::Minutes{minute};
  }

  const auto hour = rng.uniformInt(6, 18);
  const auto minute = rng.uniformInt(0, 60);
  return dueTs + time::Hours{hour} + time::Minutes{minute};
}

} // namespace PhantomLedger::transfers::obligations::jitter
