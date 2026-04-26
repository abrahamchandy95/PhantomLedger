#pragma once

#include "phantomledger/primitives/validate/checks.hpp"

namespace PhantomLedger::spending::config {

struct ExplorationHabits {
  double alpha = 1.6;
  double beta = 9.5;
  double weekendMultiplier = 1.25;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::gt("alpha", alpha, 0.0); });
    r.check([&] { v::gt("beta", beta, 0.0); });
    r.check([&] { v::gt("weekendMultiplier", weekendMultiplier, 0.0); });
  }
};

inline constexpr ExplorationHabits kDefaultExploration{};

} // namespace PhantomLedger::spending::config
