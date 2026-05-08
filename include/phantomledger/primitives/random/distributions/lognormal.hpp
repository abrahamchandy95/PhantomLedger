#pragma once

#include "normal.hpp"
#include "phantomledger/primitives/random/rng.hpp"

#include <cmath>
#include <stdexcept>

namespace PhantomLedger::probability::distributions {

/// Sample from LogNormal parameterized by the median.
///
/// median(X) = exp(mu), so mu = log(median).
[[nodiscard]] inline double lognormalByMedian(random::Rng &rng, double median,
                                              double sigma) {
  if (median <= 0.0) {
    throw std::invalid_argument("lognormalByMedian: median must be > 0");
  }
  if (sigma < 0.0) {
    throw std::invalid_argument("lognormalByMedian: sigma must be >= 0");
  }

  const double mu = std::log(median);
  return std::exp(mu + sigma * standardNormal(rng));
}

/// Sample from LogNormal parameterized by mean and sigma (NumPy convention).
///
/// X ~ LogNormal(mean, sigma) means log(X) ~ Normal(mean, sigma^2).
[[nodiscard]] inline double lognormal(random::Rng &rng, double mean,
                                      double sigma) {
  if (sigma < 0.0) {
    throw std::invalid_argument("lognormal: sigma must be >= 0");
  }

  return std::exp(mean + sigma * standardNormal(rng));
}

} // namespace PhantomLedger::probability::distributions
