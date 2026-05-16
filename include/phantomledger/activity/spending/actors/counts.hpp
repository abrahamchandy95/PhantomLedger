#pragma once

#include "phantomledger/activity/spending/actors/spender.hpp"
#include "phantomledger/activity/spending/liquidity/factor.hpp"
#include "phantomledger/math/counts.hpp"
#include "phantomledger/primitives/random/rng.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>

namespace PhantomLedger::activity::spending::actors {

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

[[nodiscard]] inline std::uint32_t sampleTransactionCount(
    random::Rng &rng, const Spender &spender, const RatePieces &ratePieces,
    std::optional<std::uint32_t> personLimit,
    const math::counts::Rates &rates = math::counts::kDefaultRates) noexcept {
  const double rate = ratePieces.baseRate * ratePieces.suppression(spender);

  if (rate <= 0.0) {
    return 0;
  }

  const int sampled = math::counts::gammaPoisson(rng, rate, rates);
  auto count = static_cast<std::uint32_t>(std::max(0, sampled));

  if (personLimit.has_value()) {
    count = std::min(count, *personLimit);
  }

  return count;
}

} // namespace PhantomLedger::activity::spending::actors
