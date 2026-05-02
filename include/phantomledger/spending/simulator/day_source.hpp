#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/math/seasonal.hpp"
#include "phantomledger/spending/actors/day.hpp"
#include "phantomledger/spending/market/bounds.hpp"
#include "phantomledger/spending/simulator/config.hpp"

#include <cstdint>

namespace PhantomLedger::spending::simulator {

class DaySource {
public:
  DaySource(DayVariation variation = {},
            math::seasonal::Config seasonal = math::seasonal::kDefaultConfig);

  [[nodiscard]] actors::DayFrame build(const market::Bounds &bounds,
                                       random::Rng &rng,
                                       std::uint32_t dayIndex) const;

private:
  DayVariation variation_{};
  math::seasonal::Config seasonal_{};
};

} // namespace PhantomLedger::spending::simulator
