#pragma once

#include "phantomledger/entropy/random/rng.hpp"

namespace PhantomLedger::probability::distributions {

/// Sample a uniform double in [low, high).
[[nodiscard]] inline double uniform(random::Rng &rng, double low, double high) {
  return low + (high - low) * rng.nextDouble();
}

} // namespace PhantomLedger::probability::distributions
