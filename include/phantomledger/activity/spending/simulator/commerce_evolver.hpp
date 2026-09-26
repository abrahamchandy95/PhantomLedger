#pragma once

#include "phantomledger/activity/spending/market/market.hpp"
#include "phantomledger/math/evolution.hpp"

#include <cstdint>

namespace PhantomLedger::activity::spending::simulator {

class CommerceEvolver {
public:
  CommerceEvolver() = default;
  explicit CommerceEvolver(math::evolution::Config config);

  /* Takes no rng (evolver-lanes-2026-09). Every draw is on the market's own
   * per-person, per-month lanes (`market.laneSeed()`), so the session rng,
   * which draws the day frames and the population dynamics, spends the same
   * number of draws whatever the catalogue, biller and favourite state. */
  void evolveIfNeeded(market::Market &market, std::uint32_t dayIndex) const;

private:
  math::evolution::Config config_{};
};

} // namespace PhantomLedger::activity::spending::simulator
