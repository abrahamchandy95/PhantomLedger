#pragma once

#include "phantomledger/spending/actors/day.hpp"
#include "phantomledger/spending/actors/spender.hpp"
#include "phantomledger/spending/config/burst.hpp"
#include "phantomledger/spending/config/exploration.hpp"
#include "phantomledger/spending/market/commerce/view.hpp"

#include <algorithm>

namespace PhantomLedger::spending::actors {

[[nodiscard]] inline double
calculateExploreP(double baseExploreP, const config::ExplorationHabits &habits,
                  const config::BurstBehavior &burst, const Spender &spender,
                  const Day &day) noexcept {
  // Map intrinsic explore propensity into a [0.25, 1.00] range so even
  // the most habit-bound spenders explore some of the time.
  constexpr double kPropOffset = 0.25;
  constexpr double kPropScale = 0.75;
  double exploreP =
      baseExploreP *
      (kPropOffset + kPropScale * static_cast<double>(spender.exploreProp));

  if (day.isWeekend) {
    exploreP *= habits.weekendMultiplier;
  }

  const bool inBurst = spender.burstStart != market::commerce::kNoBurstDay &&
                       spender.burstLen > 0 &&
                       day.dayIndex >= spender.burstStart &&
                       day.dayIndex < spender.burstStart + spender.burstLen;

  if (inBurst) {
    exploreP *= burst.multiplier;
  }

  // Hard ceiling: never explore more than half the time.
  constexpr double kExploreCeiling = 0.50;
  return std::clamp(exploreP, 0.0, kExploreCeiling);
}

} // namespace PhantomLedger::spending::actors
