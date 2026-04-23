#pragma once

#include "phantomledger/entropy/random/rng.hpp"

#include <cmath>
#include <cstdint>

namespace PhantomLedger::probability::distributions {

/// Sample from Poisson(lambda).
///
/// Knuth's method for lambda < 30, transformed rejection for larger.
[[nodiscard]] inline std::int64_t poisson(random::Rng &rng, double lambda) {
  if (lambda <= 0.0) {
    return 0;
  }

  if (lambda < 30.0) {
    const double L = std::exp(-lambda);
    std::int64_t k = 0;
    double p = 1.0;
    do {
      ++k;
      p *= rng.nextDouble();
    } while (p > L);
    return k - 1;
  }

  // Transformed rejection (Ahrens-Dieter).
  const double sqrtLam = std::sqrt(lambda);
  const double logLam = std::log(lambda);
  const double b = 0.931 + 2.53 * sqrtLam;
  const double a = -0.059 + 0.02483 * b;
  const double invAlpha = 1.1239 + 1.1328 / (b - 3.4);
  const double vr = 0.9277 - 3.6224 / (b - 2.0);

  for (;;) {
    const double u = rng.nextDouble() - 0.5;
    const double v = rng.nextDouble();
    const double us = 0.5 - std::abs(u);

    auto k = static_cast<std::int64_t>(
        std::floor((2.0 * a / us + b) * u + lambda + 0.43));

    if (k < 0) {
      continue;
    }

    if (us >= 0.07 && v <= vr) {
      return k;
    }

    if (us < 0.013 && v > us) {
      continue;
    }

    const double kf = static_cast<double>(k);
    if (std::log(v * invAlpha / (a / (us * us) + b)) <=
        -lambda + kf * logLam - std::lgamma(kf + 1.0)) {
      return k;
    }
  }
}

} // namespace PhantomLedger::probability::distributions
