#pragma once

#include <cstdint>

namespace PhantomLedger::spending::routing {

struct Policy {
  double preferBillersP = 0.85;
  std::uint16_t maxRetries = 6;
};

} // namespace PhantomLedger::spending::routing
