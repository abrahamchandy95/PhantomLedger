#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

#include <algorithm>
#include <cstdint>

namespace PhantomLedger::transfers::subscriptions {

/// Day-of-month bounds.
inline constexpr int kDayMin = 1;
inline constexpr int kDayMax = 28;

/// Within-day jitter ranges
inline constexpr int kHourLow = 0;
inline constexpr int kHourHighExcl = 6; // [0, 5]
inline constexpr int kMinuteLow = 0;
inline constexpr int kMinuteHighExcl = 60; // [0, 59]

[[nodiscard]] inline std::int64_t
cycleTimestamp(random::Rng &rng, time::TimePoint monthStart,
               std::uint8_t baseDay, std::uint8_t dayJitter) noexcept {
  const auto jitter =
      (dayJitter == 0)
          ? std::int64_t{0}
          : rng.uniformInt(-static_cast<std::int64_t>(dayJitter),
                           static_cast<std::int64_t>(dayJitter) + 1);
  const int rawDay = static_cast<int>(baseDay) + static_cast<int>(jitter);
  const int day = std::clamp(rawDay, kDayMin, kDayMax);

  const auto hour = rng.uniformInt(kHourLow, kHourHighExcl);
  const auto minute = rng.uniformInt(kMinuteLow, kMinuteHighExcl);

  const auto tp = monthStart + time::Days{day - 1} + time::Hours{hour} +
                  time::Minutes{minute};
  return time::toEpochSeconds(tp);
}

} // namespace PhantomLedger::transfers::subscriptions
