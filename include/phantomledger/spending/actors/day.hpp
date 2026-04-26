#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

#include <cstdint>

namespace PhantomLedger::spending::actors {

struct Day {
  std::uint32_t dayIndex = 0;
  time::TimePoint start{};
  std::uint8_t weekday = 0; // 0 = Monday, ..., 6 = Sunday
  bool isWeekend = false;
  double shock = 1.0; // multiplicative noise around the day's expectation
};

[[nodiscard]] Day buildDay(time::TimePoint windowStart, double dayShockShape,
                           random::Rng &rng, std::uint32_t dayIndex);

} // namespace PhantomLedger::spending::actors
