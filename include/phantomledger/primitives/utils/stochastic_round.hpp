#pragma once

#include <cstdint>

namespace PhantomLedger::primitives::utils {

[[nodiscard]] inline std::uint32_t stochasticRound(double x,
                                                   double u) noexcept {
  if (!(x > 0.0)) {
    return 0;
  }
  const auto whole = static_cast<std::uint32_t>(x);
  const double frac = x - static_cast<double>(whole);
  return whole + (u < frac ? 1u : 0u);
}

} // namespace PhantomLedger::primitives::utils
