#pragma once

#include "phantomledger/primitives/time/calendar.hpp"

#include <cstdint>

namespace PhantomLedger::spending::market {

struct Bounds {
  time::TimePoint startDate{};
  std::uint32_t days = 0;

  [[nodiscard]] time::TimePoint end() const noexcept {
    return time::addDays(startDate, static_cast<int>(days));
  }

  [[nodiscard]] bool contains(time::TimePoint ts) const noexcept {
    return ts >= startDate && ts < end();
  }
};

} // namespace PhantomLedger::spending::market
