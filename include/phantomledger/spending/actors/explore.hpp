#pragma once

#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/spending/actors/day.hpp"
#include "phantomledger/spending/actors/spender.hpp"
#include "phantomledger/spending/market/commerce/view.hpp"

#include <algorithm>

namespace PhantomLedger::spending::actors {

struct ExploreModifiers {
  double weekendMultiplier = 1.25;
  double burstMultiplier = 3.25;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::gt("weekendMultiplier", weekendMultiplier, 0.0); });
    r.check([&] { v::gt("burstMultiplier", burstMultiplier, 0.0); });
  }
};

[[nodiscard]] inline double calculateExploreP(double baseExploreP,
                                              const ExploreModifiers &modifiers,
                                              const Spender &spender,
                                              const Day &day) noexcept {
  constexpr double kPropOffset = 0.25;
  constexpr double kPropScale = 0.75;
  double exploreP =
      baseExploreP *
      (kPropOffset + kPropScale * static_cast<double>(spender.exploreProp));

  if (day.isWeekend) {
    exploreP *= modifiers.weekendMultiplier;
  }

  const bool inBurst = spender.burstStart != market::commerce::kNoBurstDay &&
                       spender.burstLen > 0 &&
                       day.dayIndex >= spender.burstStart &&
                       day.dayIndex < spender.burstStart + spender.burstLen;

  if (inBurst) {
    exploreP *= modifiers.burstMultiplier;
  }

  constexpr double kExploreCeiling = 0.50;
  return std::clamp(exploreP, 0.0, kExploreCeiling);
}

} // namespace PhantomLedger::spending::actors
