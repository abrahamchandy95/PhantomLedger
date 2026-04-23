#pragma once

#include "normal.hpp"
#include "phantomledger/entropy/random/rng.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace PhantomLedger::probability::distributions {

/// Sample from Binomial(n, p).
///
/// Repeated Bernoulli for small n; normal approximation for large n.
[[nodiscard]] inline std::int64_t binomial(random::Rng &rng, std::int64_t n,
                                           double p) {
  if (n <= 0 || p <= 0.0) {
    return 0;
  }
  if (p >= 1.0) {
    return n;
  }

  if (n <= 64) {
    std::int64_t count = 0;
    for (std::int64_t i = 0; i < n; ++i) {
      if (rng.nextDouble() < p) {
        ++count;
      }
    }
    return count;
  }

  // Normal approximation for large n.
  const double mean = static_cast<double>(n) * p;
  const double stddev = std::sqrt(mean * (1.0 - p));
  const auto result =
      static_cast<std::int64_t>(std::round(normal(rng, mean, stddev)));

  return std::max(std::int64_t{0}, std::min(n, result));
}

} // namespace PhantomLedger::probability::distributions
