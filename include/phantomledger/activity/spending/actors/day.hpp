#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

#include <cstdint>

namespace PhantomLedger::spending::actors {

struct Day {
  std::uint32_t dayIndex = 0;
  time::TimePoint start{};
  std::uint8_t weekday = 0;
  bool isWeekend = false;
  double shock = 1.0;
};

struct DayFrame {
  Day day{};
  double weekdayMult = 1.0;
  double seasonalMult = 1.0;
};

[[nodiscard]] Day buildDay(time::TimePoint windowStart, double dayShockShape,
                           random::Rng &rng, std::uint32_t dayIndex);

} // namespace PhantomLedger::spending::actors
