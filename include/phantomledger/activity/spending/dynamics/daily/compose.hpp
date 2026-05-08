#pragma once

#include <cstddef>
#include <span>

namespace PhantomLedger::spending::dynamics::daily {

inline void composeAll(std::span<const double> momentum,
                       std::span<const double> dormancy,
                       std::span<const double> paycheck, double seasonalMult,
                       std::span<double> out) noexcept {
  const auto n = momentum.size();
  for (std::size_t i = 0; i < n; ++i) {
    out[i] = momentum[i] * dormancy[i] * paycheck[i] * seasonalMult;
  }
}

} // namespace PhantomLedger::spending::dynamics::daily
