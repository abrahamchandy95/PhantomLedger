#pragma once

#include "gamma.hpp"
#include "phantomledger/primitives/random/rng.hpp"

#include <stdexcept>

namespace PhantomLedger::probability::distributions {

/// Sample from Beta(alpha, beta) via the ratio of independent gammas.
[[nodiscard]] inline double beta(random::Rng &rng, double alpha,
                                 double betaParam) {
  if (alpha <= 0.0 || betaParam <= 0.0) {
    throw std::invalid_argument("beta: alpha and beta must be > 0");
  }

  const double x = gamma(rng, alpha, 1.0);
  const double y = gamma(rng, betaParam, 1.0);

  return x / (x + y);
}

} // namespace PhantomLedger::probability::distributions
