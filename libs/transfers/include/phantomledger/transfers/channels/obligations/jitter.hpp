#pragma once

#include "phantomledger/primitives/random/rng.hpp"
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

/// Late-payment behavior for a single installment
struct LateSpec {
  double lateP = 0.0;
  std::int32_t daysMin = 0;
  std::int32_t daysMax = 0;
  bool forceLate = false;

  [[nodiscard]] bool shouldBeLate(random::Rng &rng) const {
    return forceLate || rng.coin(lateP);
  }

  [[nodiscard]] std::int32_t sampleDelayDays(random::Rng &rng) const {
    if (daysMax <= 0) {
      return 0;
    }
    return static_cast<std::int32_t>(rng.uniformInt(daysMin, daysMax + 1));
  }
};

/// Timestamp for a realized installment payment.
[[nodiscard]] inline time::TimePoint
installmentTimestamp(random::Rng &rng, time::TimePoint dueTs,
                     const LateSpec &late) {
  if (late.shouldBeLate(rng)) {
    const auto delayDays = late.sampleDelayDays(rng);
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
