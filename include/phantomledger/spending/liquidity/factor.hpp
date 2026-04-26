#pragma once

#include <algorithm>

namespace PhantomLedger::spending::liquidity {

[[nodiscard]] constexpr double countFactor(double liquidityMult) noexcept {
  // Match Python: clamp to [0, 1.25], soft-shape below 1.0, square.
  const double liq = std::clamp(liquidityMult, 0.0, 1.25);
  const double softened = liq <= 1.0 ? (0.50 + 0.50 * liq) : liq;
  return softened * softened;
}

} // namespace PhantomLedger::spending::liquidity
