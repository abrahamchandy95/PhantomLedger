#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/spending/actors/spender.hpp"
#include "phantomledger/transactions/factory.hpp"

namespace PhantomLedger::spending::actors {

struct Event {
  const Spender *spender = nullptr;
  const transactions::Factory *factory = nullptr;
  time::TimePoint ts{};
  double exploreP = 0.0;
};

} // namespace PhantomLedger::spending::actors
