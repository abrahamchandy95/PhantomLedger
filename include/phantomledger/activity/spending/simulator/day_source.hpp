#pragma once

#include "phantomledger/activity/spending/actors/day.hpp"
#include "phantomledger/activity/spending/market/bounds.hpp"
#include "phantomledger/math/seasonal.hpp"
#include "phantomledger/primitives/random/rng.hpp"

#include <cstdint>

namespace PhantomLedger::activity::spending::simulator {

class DaySource {
public:
  struct Variation {

    double shockShape = 1.3;
  };

  DaySource();
  explicit DaySource(Variation variation);
  DaySource(Variation variation, math::seasonal::Config seasonal);

  [[nodiscard]] actors::DayFrame build(const market::Bounds &bounds,
                                       random::Rng &rng,
                                       std::uint32_t dayIndex) const;

private:
  Variation variation_;
  math::seasonal::Config seasonal_;
};

} // namespace PhantomLedger::activity::spending::simulator
