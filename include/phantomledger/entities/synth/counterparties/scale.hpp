#pragma once

#include <algorithm>
#include <cmath>

namespace PhantomLedger::entities::synth::counterparties {

[[nodiscard]] inline int scale(double perTenK, int population, int floor) {
  const int scaled = static_cast<int>(
      std::round(perTenK * (static_cast<double>(population) / 10000.0)));
  return std::max(floor, scaled);
}

} // namespace PhantomLedger::entities::synth::counterparties
