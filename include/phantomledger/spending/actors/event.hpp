#pragma once

#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/spending/actors/spender.hpp"

namespace PhantomLedger::spending::actors {

struct Event {
  const Spender *spender = nullptr;
  time::TimePoint ts{};
  double exploreP = 0.0;
};

} // namespace PhantomLedger::spending::actors
