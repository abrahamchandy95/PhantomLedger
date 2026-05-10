#pragma once

#include "phantomledger/activity/spending/actors/spender.hpp"
#include "phantomledger/activity/spending/liquidity/factor.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>

namespace PhantomLedger::spending::actors {

struct RatePieces {
  double baseRate = 0.0;
  double weekdayMult = 1.0;
  double dynamicsMultiplier = 1.0;
  double liquidityMultiplier = 1.0;
  double dayShock = 1.0;

  [[nodiscard]] double suppression(const Spender &spender) const noexcept {
    const double dyn = std::max(0.0, dynamicsMultiplier);
    return spender.rateMultiplier * weekdayMult * dayShock * dyn *
           liquidity::countFactor(liquidityMultiplier);
  }
};

[[nodiscard]] inline std::uint32_t
sampleTransactionCount(random::Rng &rng, const Spender &spender,
                       const RatePieces &ratePieces,
                       std::optional<std::uint32_t> personLimit) noexcept {
  const double rate = ratePieces.baseRate * ratePieces.suppression(spender);

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
