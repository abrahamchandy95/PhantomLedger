#pragma once

#include "normal.hpp"
#include "phantomledger/primitives/random/rng.hpp"

#include <cmath>
#include <stdexcept>

namespace PhantomLedger::probability::distributions {

/// Sample from Gamma(shape, scale) via Marsaglia & Tsang's method.
[[nodiscard]] inline double gamma(random::Rng &rng, double shape,
                                  double scale) {
  if (shape <= 0.0 || scale <= 0.0) {
    throw std::invalid_argument("gamma: shape and scale must be > 0");
  }

  double adjShape = shape;
  double boost = 1.0;

  if (adjShape < 1.0) {
    // Ahrens-Dieter: Gamma(a) = Gamma(a+1) * U^(1/a) for a < 1.
    boost = std::pow(rng.nextDouble(), 1.0 / adjShape);
    adjShape += 1.0;
  }

  const double d = adjShape - 1.0 / 3.0;
  const double c = 1.0 / std::sqrt(9.0 * d);

  for (;;) {
    const double x = standardNormal(rng);
    double v = 1.0 + c * x;

    if (v <= 0.0) {
      continue;
    }

    v = v * v * v;

    double u = rng.nextDouble();
    if (u < 1e-300) {
      u = 1e-300;
    }

    if (u < 1.0 - 0.0331 * (x * x) * (x * x)) {
      return d * v * scale * boost;
    }

    if (std::log(u) < 0.5 * x * x + d * (1.0 - v + std::log(v))) {
      return d * v * scale * boost;
    }
  }
}

} // namespace PhantomLedger::probability::distributions
