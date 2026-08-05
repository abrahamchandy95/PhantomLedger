#pragma once

#include "phantomledger/activity/spending/actors/day.hpp"
#include "phantomledger/activity/spending/actors/spender.hpp"
#include "phantomledger/activity/spending/market/commerce/view.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

#include <algorithm>

namespace PhantomLedger::activity::spending::actors {

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

  // burst-rate-2026-07: a person may hold several burst windows over a long
  // run, so this is a scan rather than a single comparison. The list is tiny
  // (a per-year rate over the window) and in day order, so the loop exits
  // early once a window starts after today.
  bool inBurst = false;
  for (const auto &window : spender.bursts) {
    if (window.startDay > day.dayIndex) {
      break;
    }
    if (window.covers(day.dayIndex)) {
      inBurst = true;
      break;
    }
  }

  if (inBurst) {
    exploreP *= modifiers.burstMultiplier;
  }

  constexpr double kExploreCeiling = 0.50;
  return std::clamp(exploreP, 0.0, kExploreCeiling);
}

} // namespace PhantomLedger::activity::spending::actors
