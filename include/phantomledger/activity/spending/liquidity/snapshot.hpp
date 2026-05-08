#pragma once

#include <cstdint>

namespace PhantomLedger::spending::liquidity {

struct Snapshot {
  std::uint32_t daysSincePayday = 0;
  double paycheckSensitivity = 0.0;
  double availableCash = 0.0;
  double baselineCash = 0.0;
  double fixedMonthlyBurden = 0.0;
};

} // namespace PhantomLedger::spending::liquidity
