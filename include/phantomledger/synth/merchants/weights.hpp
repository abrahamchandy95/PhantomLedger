#pragma once

#include <cmath>
#include <vector>

namespace PhantomLedger::synth::merchants {

[[nodiscard]] inline std::vector<double>
normalize(const std::vector<double> &raw) {
  if (raw.empty()) {
    return {};
  }

  double sum = 0.0;
  for (double value : raw) {
    sum += value;
  }

  std::vector<double> out(raw.size());

  if (!std::isfinite(sum) || sum <= 0.0) {
    const double uniform = 1.0 / static_cast<double>(raw.size());
    for (double &value : out) {
      value = uniform;
    }
    return out;
  }

  const double invSum = 1.0 / sum;
  for (std::size_t i = 0; i < raw.size(); ++i) {
    out[i] = raw[i] * invSum;
  }

  return out;
}

} // namespace PhantomLedger::synth::merchants
