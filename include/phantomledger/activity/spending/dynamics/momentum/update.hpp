#pragma once

#include "phantomledger/activity/spending/dynamics/momentum/state.hpp"
#include "phantomledger/primitives/random/rng.hpp"

#include <span>

namespace PhantomLedger::spending::dynamics::momentum {

inline void accumulate(random::Rng &rng, const Config &cfg,
                       std::span<State> states,
                       std::span<double> outAccum) noexcept {
  const auto n = states.size();
  for (std::size_t i = 0; i < n; ++i) {
    outAccum[i] *= states[i].advance(rng, cfg);
  }
}

} // namespace PhantomLedger::spending::dynamics::momentum
