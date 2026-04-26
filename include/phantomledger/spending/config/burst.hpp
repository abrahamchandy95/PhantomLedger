#pragma once

#include "phantomledger/primitives/validate/checks.hpp"

#include <cstdint>

namespace PhantomLedger::spending::config {

struct BurstBehavior {
  double probability = 0.08;
  std::uint16_t minDays = 3;
  std::uint16_t maxDays = 9;
  double multiplier = 3.25;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::unit("probability", probability); });
    r.check([&] { v::ge("minDays", static_cast<int>(minDays), 1); });
    r.check([&] {
      v::ge("maxDays", static_cast<int>(maxDays), static_cast<int>(minDays));
    });
    r.check([&] { v::gt("multiplier", multiplier, 0.0); });
  }
};

inline constexpr BurstBehavior kDefaultBurst{};

} // namespace PhantomLedger::spending::config
