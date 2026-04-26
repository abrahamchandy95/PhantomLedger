#pragma once

#include "phantomledger/primitives/validate/checks.hpp"

#include <cstdint>

namespace PhantomLedger::spending::config {

struct LiquidityConstraints {
  bool enabled = true;
  std::uint16_t reliefDays = 2;
  std::uint16_t stressStartDay = 3;
  std::uint16_t stressRampDays = 5;
  double absoluteFloor = 0.08;
  double explorationFloor = 0.0;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::ge("reliefDays", static_cast<int>(reliefDays), 0); });
    r.check(
        [&] { v::ge("stressStartDay", static_cast<int>(stressStartDay), 0); });
    r.check(
        [&] { v::ge("stressRampDays", static_cast<int>(stressRampDays), 1); });
    r.check([&] { v::unit("absoluteFloor", absoluteFloor); });
    r.check([&] { v::unit("explorationFloor", explorationFloor); });
  }
};

inline constexpr LiquidityConstraints kDefaultLiquidityConstraints{};

inline constexpr double kLiquidityCeiling = 1.10;

} // namespace PhantomLedger::spending::config
