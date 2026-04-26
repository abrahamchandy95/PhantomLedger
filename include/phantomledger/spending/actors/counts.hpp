#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/utils/stochastic_round.hpp"
#include "phantomledger/spending/actors/day.hpp"
#include "phantomledger/spending/actors/spender.hpp"
#include "phantomledger/spending/liquidity/factor.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>

namespace PhantomLedger::spending::actors {

[[nodiscard]] inline std::uint32_t
sampleTxnCount(random::Rng &rng, const Spender &spender, const Day &day,
               double baseRate, double weekdayMult,
               std::optional<std::uint32_t> personLimit,
               double dynamicsMultiplier, double liquidityMultiplier) noexcept {
  const double dyn = std::max(0.0, dynamicsMultiplier);
  const double rate = baseRate * spender.rateMultiplier * weekdayMult *
                      day.shock * dyn *
                      liquidity::countFactor(liquidityMultiplier);

  if (rate <= 0.0) {
    return 0;
  }

  std::uint32_t count =
      primitives::utils::stochasticRound(rate, rng.nextDouble());

  if (personLimit.has_value()) {
    count = std::min(count, *personLimit);
  }
  return count;
}

} // namespace PhantomLedger::spending::actors
