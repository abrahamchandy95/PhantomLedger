#pragma once

#include <algorithm>

namespace PhantomLedger::activity::spending::liquidity {

[[nodiscard]] constexpr double countFactor(double liquidityMult) noexcept {
  const double liq = std::clamp(liquidityMult, 0.0, 1.25);
  const double softened = liq <= 1.0 ? (0.50 + 0.50 * liq) : liq;
  return softened * softened;
}

[[nodiscard]] constexpr double amountFactor(double liquidityMult) noexcept {
  constexpr double kHighWater = 0.95;
  constexpr double kLowWater = 0.70;
  constexpr double kFloor = 0.85;

  if (liquidityMult >= kHighWater) {
    return 1.0;
  }

  if (liquidityMult <= kLowWater) {
    return kFloor;
  }

  const double t = (liquidityMult - kLowWater) / (kHighWater - kLowWater);
  return kFloor + (1.0 - kFloor) * t;
}

} // namespace PhantomLedger::activity::spending::liquidity
