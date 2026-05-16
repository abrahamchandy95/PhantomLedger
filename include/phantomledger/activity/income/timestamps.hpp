#pragma once

#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

namespace PhantomLedger::activity::income::timestamps {

struct TimestampJitter {
  int dayOffsetMax = 0;
  int hourStart = 0;
  int hourEndExcl = 24;

  [[nodiscard]] constexpr bool valid() const noexcept {
    return dayOffsetMax >= 0 && hourStart >= 0 && hourStart < hourEndExcl &&
           hourEndExcl <= 24;
  }
};

[[nodiscard]] inline time::TimePoint jittered(time::TimePoint baseDate,
                                              int postingLagDays,
                                              const TimestampJitter &jitter,
                                              random::Rng &rng) {
  auto ts = time::addDays(baseDate, postingLagDays);

  if (jitter.dayOffsetMax > 0) {
    ts = time::addDays(
        ts, static_cast<int>(rng.uniformInt(0, jitter.dayOffsetMax)));
  }

  const int hour =
      static_cast<int>(rng.uniformInt(jitter.hourStart, jitter.hourEndExcl));
  const int minute = static_cast<int>(rng.uniformInt(0, 60));

  return ts + time::Hours{hour} + time::Minutes{minute};
}

inline constexpr TimestampJitter kSalaryTimestampJitter{
    .dayOffsetMax = 0,
    .hourStart = 6,
    .hourEndExcl = 12,
};

inline constexpr TimestampJitter kRentTimestampJitter{
    .dayOffsetMax = 6,
    .hourStart = 7,
    .hourEndExcl = 22,
};

} // namespace PhantomLedger::activity::income::timestamps
