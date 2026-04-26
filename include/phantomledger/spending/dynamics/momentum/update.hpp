#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/spending/dynamics/momentum/state.hpp"

#include <span>

namespace PhantomLedger::spending::dynamics::momentum {

inline void advanceAll(random::Rng &rng, const Config &cfg,
                       std::span<State> states,
                       std::span<double> outMultipliers) noexcept {
  const auto n = states.size();
  for (std::size_t i = 0; i < n; ++i) {
    outMultipliers[i] = states[i].advance(rng, cfg);
  }
}

} // namespace PhantomLedger::spending::dynamics::momentum
