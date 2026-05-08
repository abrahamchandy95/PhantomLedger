#pragma once

#include "phantomledger/primitives/random/rng.hpp"

#include <cmath>
#include <numbers>

namespace PhantomLedger::probability::distributions {

/// Sample one variate from the standard normal distribution N(0, 1).
///
/// Uses Box-Muller: two uniform draws produce one standard normal.
/// We discard the second variate for simplicity; the hot path draws
/// one value at a time and the PCG64 is fast enough that wasting
/// one draw is cheaper than maintaining paired state.
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
