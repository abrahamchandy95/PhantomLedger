#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/probability/distributions/normal.hpp"

#include <algorithm>

namespace PhantomLedger::math::momentum {

struct Config {
  double phi = 0.45;
  double noiseSigma = 0.15;
  double floor = 0.20;
  double ceiling = 3.00;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::between("phi", phi, 0.0, 0.99); });
    r.check([&] { v::nonNegative("noiseSigma", noiseSigma); });
    r.check([&] { v::between("floor", floor, 0.01, 1.0); });
    r.check([&] { v::between("ceiling", ceiling, 1.0, 10.0); });
  }
};

inline constexpr Config kDefaultConfig{};

struct State {
  double value = 1.0;

  double advance(random::Rng &rng, const Config &cfg) {
    const double meanRevert = cfg.phi * value + (1.0 - cfg.phi);
    const double eps =
        cfg.noiseSigma > 0.0
            ? probability::distributions::normal(rng, 0.0, cfg.noiseSigma)
            : 0.0;
    value = std::clamp(meanRevert + eps, cfg.floor, cfg.ceiling);
    return value;
  }
};

} // namespace PhantomLedger::math::momentum
