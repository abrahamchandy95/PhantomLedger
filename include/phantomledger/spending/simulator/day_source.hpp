#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/math/seasonal.hpp"
#include "phantomledger/spending/actors/day.hpp"
#include "phantomledger/spending/market/bounds.hpp"

#include <cstdint>

namespace PhantomLedger::spending::simulator {

class DaySource {
public:
  struct Variation {
    double shockShape = 1.0;
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

} // namespace PhantomLedger::spending::simulator
