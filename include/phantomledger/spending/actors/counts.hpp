#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/math/counts.hpp"
#include "phantomledger/spending/actors/day.hpp"
#include "phantomledger/spending/actors/spender.hpp"
#include "phantomledger/spending/liquidity/factor.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>

namespace PhantomLedger::spending::actors {

[[nodiscard]] inline std::uint32_t
sampleTxnCount(random::Rng &rng, const Spender &spender, const Day &day,
               double baseRate, std::optional<std::uint32_t> personLimit,
               double dynamicsMultiplier, double liquidityMultiplier) {
  const double dyn = std::max(0.0, dynamicsMultiplier);
  const double rate = baseRate * spender.persona->cash.rateMultiplier *
                      math::counts::weekdayMultiplier(day.start) * day.shock *
                      dyn * liquidity::countFactor(liquidityMultiplier);

  if (rate <= 0.0) {
    return 0;
  }

  const auto whole = static_cast<std::uint32_t>(rate);
  const double frac = rate - static_cast<double>(whole);

  std::uint32_t count = whole;
  if (rng.nextDouble() < frac) {
    ++count;
  }

  if (personLimit.has_value()) {
    count = std::min(count, *personLimit);
  }
  return count;
}

} // namespace PhantomLedger::spending::actors
