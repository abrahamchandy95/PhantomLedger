#pragma once

#include "phantomledger/activity/spending/dynamics/dormancy/state.hpp"
#include "phantomledger/entropy/random/rng.hpp"

#include <span>

namespace PhantomLedger::spending::dynamics::dormancy {

inline void accumulate(random::Rng &rng, const Config &cfg,
                       std::span<State> states,
                       std::span<double> outAccum) noexcept {
  const auto n = states.size();
  for (std::size_t i = 0; i < n; ++i) {
    outAccum[i] *= states[i].advance(rng, cfg);
  }
}

} // namespace PhantomLedger::spending::dynamics::dormancy
