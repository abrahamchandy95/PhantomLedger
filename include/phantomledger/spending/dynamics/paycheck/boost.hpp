#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/math/paycheck.hpp"

#include <span>

namespace PhantomLedger::spending::dynamics::paycheck {

using State = math::paycheck::State;
using Config = math::paycheck::Config;

inline constexpr Config kDefaultConfig = math::paycheck::kDefaultConfig;

inline void
triggerForPaydays(const Config &cfg, std::span<State> states,
                  std::span<const double> sensitivities,
                  std::span<const std::uint32_t> paydayPersonIndices) {
  for (const auto idx : paydayPersonIndices) {
    states[idx].trigger(cfg.boostForSensitivity(sensitivities[idx]), cfg);
  }
}

inline void advanceAll(std::span<State> states,
                       std::span<double> outMultipliers) noexcept {
  const auto n = states.size();
  for (std::size_t i = 0; i < n; ++i) {
    outMultipliers[i] = states[i].advance();
  }
}

} // namespace PhantomLedger::spending::dynamics::paycheck
