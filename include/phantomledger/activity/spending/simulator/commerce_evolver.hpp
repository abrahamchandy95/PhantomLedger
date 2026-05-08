#pragma once

#include "phantomledger/activity/spending/market/market.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/math/evolution.hpp"

#include <cstdint>

namespace PhantomLedger::spending::simulator {

class CommerceEvolver {
public:
  CommerceEvolver() = default;
  explicit CommerceEvolver(math::evolution::Config config);

  void evolveIfNeeded(market::Market &market, random::Rng &rng,
                      std::uint32_t dayIndex) const;

private:
  math::evolution::Config config_{};
};

} // namespace PhantomLedger::spending::simulator
