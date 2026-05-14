#pragma once

#include <cstdint>

namespace PhantomLedger::time {

inline constexpr std::int64_t kSecondsPerMinute = 60;
inline constexpr std::int64_t kSecondsPerHour = 3600;
inline constexpr std::int64_t kSecondsPerDay = 86'400;

/// Compose `h:m:s` as a count of seconds past midnight.
[[nodiscard]] constexpr std::int64_t
secondsInDay(std::int64_t h, std::int64_t m, std::int64_t s = 0) noexcept {
  return h * kSecondsPerHour + m * kSecondsPerMinute + s;
}

} // namespace PhantomLedger::time
