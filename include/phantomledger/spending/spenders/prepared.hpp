#pragma once

#include "phantomledger/spending/actors/spender.hpp"

#include <cstdint>
#include <span>

namespace PhantomLedger::spending::spenders {

struct PreparedSpender {
  actors::Spender spender{};

  std::span<const std::uint32_t> paydays{};

  double initialCash = 0.0;
  double baselineCash = 0.0;
  double fixedBurden = 0.0;
  double paycheckSensitivity = 0.0;
};

} // namespace PhantomLedger::spending::spenders
