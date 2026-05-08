#pragma once

#include "phantomledger/math/paycheck.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace PhantomLedger::spending::dynamics::paycheck {

using State = math::paycheck::State;
using Config = math::paycheck::Config;

inline constexpr Config kDefaultConfig = math::paycheck::kDefaultConfig;

/// Activate or refresh the paycheck boost for every person who received
/// a paycheck today. `paydayPersonIndices` is sourced from the
/// pre-built `PaydayIndex::personsOn(dayIndex)` — small relative to N.
inline void
triggerForPaydays(const Config &cfg, std::span<State> states,
                  std::span<const double> sensitivities,
                  std::span<const std::uint32_t> paydayPersonIndices) {
  for (const auto idx : paydayPersonIndices) {
    states[idx].trigger(cfg.boostForSensitivity(sensitivities[idx]), cfg);
  }
}

/// Advance every paycheck state by one day and multiply the boost
/// multiplier into `outAccum` in place.
inline void accumulate(std::span<State> states,
                       std::span<double> outAccum) noexcept {
  const auto n = states.size();
  for (std::size_t i = 0; i < n; ++i) {
    outAccum[i] *= states[i].advance();
  }
}

} // namespace PhantomLedger::spending::dynamics::paycheck
