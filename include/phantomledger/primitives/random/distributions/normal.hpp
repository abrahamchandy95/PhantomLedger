#pragma once

#include "phantomledger/primitives/random/rng.hpp"

#include <cmath>
#include <numbers>

namespace PhantomLedger::probability::distributions {

[[nodiscard]] inline double standardNormal(random::Rng &rng) {
  double u1 = rng.nextDouble();
  const double u2 = rng.nextDouble();

  if (u1 < 1e-300) {
    u1 = 1e-300;
  }

  return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * std::numbers::pi * u2);
}

/// Sample one variate from N(mean, stddev).
[[nodiscard]] inline double normal(random::Rng &rng, double mean,
                                   double stddev) {
  return mean + stddev * standardNormal(rng);
}

} // namespace PhantomLedger::probability::distributions
