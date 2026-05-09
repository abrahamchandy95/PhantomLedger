#pragma once

#include "phantomledger/primitives/validate/checks.hpp"

#include <cstdint>

namespace PhantomLedger::spending::routing {

struct PaymentRoutingRules {
  double preferKnownBillersP = 0.85;
  std::uint16_t merchantRetryLimit = 6;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::unit("preferKnownBillersP", preferKnownBillersP); });
  }
};

inline constexpr PaymentRoutingRules kDefaultPaymentRoutingRules{};

} // namespace PhantomLedger::spending::routing
