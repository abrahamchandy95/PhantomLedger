#pragma once

#include "phantomledger/spending/market/population/view.hpp"

#include <cstdint>
#include <span>

namespace PhantomLedger::spending::dynamics::daily {

struct Ctx {
  std::uint32_t dayIndex = 0;

  std::span<const double> sensitivities;

  const market::population::View *population = nullptr;
};

} // namespace PhantomLedger::spending::dynamics::daily
