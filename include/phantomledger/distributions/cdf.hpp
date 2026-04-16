#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <vector>

namespace PhantomLedger::distributions {

namespace detail {

[[nodiscard]] inline std::size_t
sampleIndex(const double *cdf, std::size_t count, double u) noexcept {
  const double *it = std::upper_bound(cdf, cdf + count, u);
  const std::size_t index = static_cast<std::size_t>(it - cdf);
  return std::min(index, count - 1);
}

} // namespace detail

[[nodiscard]] inline std::vector<double>
buildCdf(std::span<const double> weights) {
  if (weights.empty()) {
    throw std::invalid_argument("buildCdf requires at least one weight");
  }

  double total = 0.0;
  for (double weight : weights) {
    if (weight < 0.0) {
      throw std::invalid_argument("buildCdf requires non-negative weights");
    }
    total += weight;
  }

  if (!std::isfinite(total) || total <= 0.0) {
    throw std::invalid_argument(
        "buildCdf requires a finite positive weight sum");
  }

  std::vector<double> cdf(weights.size());
  double sum = 0.0;

  for (std::size_t i = 0; i < weights.size(); ++i) {
    sum += weights[i] / total;
    cdf[i] = sum;
  }

  cdf.back() = 1.0;
  return cdf;
}

[[nodiscard]] inline std::size_t sampleIndex(std::span<const double> cdf,
                                             double u) {
  if (cdf.empty()) {
    throw std::invalid_argument("sampleIndex requires a non-empty cdf");
  }

  return detail::sampleIndex(cdf.data(), cdf.size(), u);
}

[[nodiscard]] inline std::size_t sampleIndex(const std::vector<double> &cdf,
                                             double u) {
  return sampleIndex(std::span<const double>{cdf}, u);
}

} // namespace PhantomLedger::distributions
