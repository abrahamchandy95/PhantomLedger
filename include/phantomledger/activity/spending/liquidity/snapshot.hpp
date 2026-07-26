#pragma once

#include <cstdint>

namespace PhantomLedger::activity::spending::liquidity {

struct Snapshot {
  std::uint32_t daysSincePayday = 0;
  double paycheckSensitivity = 0.0;
  double availableToSpend = 0.0;
  double baselineCash = 0.0;
  double fixedMonthlyBurden = 0.0;
  // H1 step 2b: the day's CPI level multiplier, so the multiplier's
  // dollar-literal cash-reference floor scales with the era it
  // screens (authority U-6 invariant-4 sweep). Pure data, no draws.
  double priceScale = 1.0;
};

} // namespace PhantomLedger::activity::spending::liquidity
