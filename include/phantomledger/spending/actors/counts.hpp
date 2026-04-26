#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/spending/actors/day.hpp"
#include "phantomledger/spending/actors/spender.hpp"
#include "phantomledger/spending/liquidity/factor.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>

namespace PhantomLedger::spending::actors {

struct RatePieces {
  double baseRate = 0.0;
  double weekdayMult = 1.0;
  double dynamicsMultiplier = 1.0;
  double liquidityMultiplier = 1.0;
};

[[nodiscard]] inline std::uint32_t
sampleTransactionCount(random::Rng &rng, const Spender &spender,
                       const DayFrame &frame, const RatePieces &ratePieces,
                       std::optional<std::uint32_t> personLimit) noexcept {
  const double dyn = std::max(0.0, ratePieces.dynamicsMultiplier);

  const double rate = ratePieces.baseRate * spender.rateMultiplier *
                      ratePieces.weekdayMult * frame.day.shock * dyn *
                      liquidity::countFactor(ratePieces.liquidityMultiplier);

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
