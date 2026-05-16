#pragma once

#include "phantomledger/activity/spending/actors/spender.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

namespace PhantomLedger::activity::spending::actors {

struct Event {
  const Spender *spender = nullptr;
  time::TimePoint ts{};
  double exploreP = 0.0;
  double availableCash = 0.0;
  double cardAvailable = 0.0;
  double amountFactor = 1.0;
};

} // namespace PhantomLedger::activity::spending::actors
