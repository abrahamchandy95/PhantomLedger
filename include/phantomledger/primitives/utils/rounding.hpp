#pragma once

#include <algorithm>
#include <cmath>

namespace PhantomLedger::primitives::utils {

/// Round a monetary amount to 2 decimal places (cents).
[[nodiscard]] inline double roundMoney(double amount) {
  return std::round(amount * 100.0) / 100.0;
}

/// Clamp a value to the closed interval [lo, hi].
[[nodiscard]] inline double clamp(double value, double lo, double hi) {
  if (value < lo) {
    return lo;
  }
  if (value > hi) {
    return hi;
  }
  return value;
}

/// Take max(floor, value) and then round to cents.
[[nodiscard]] inline double floorAndRound(double value, double floor) {
  return roundMoney(std::max(floor, value));
}

} // namespace PhantomLedger::primitives::utils
