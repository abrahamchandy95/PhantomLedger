#pragma once

#include "phantomledger/primitives/validate/checks.hpp"

#include <algorithm>
#include <cstdint>

namespace PhantomLedger::math::paycheck {

struct Config {
  bool enabled = true;
  double maxResidualBoost = 0.10;
  int activeDays = 4;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::unit("maxResidualBoost", maxResidualBoost); });
    r.check([&] { v::ge("activeDays", activeDays, 1); });
  }

  [[nodiscard]] constexpr double
  boostForSensitivity(double sensitivity) const noexcept {
    if (!enabled) {
      return 0.0;
    }
    return std::clamp(sensitivity, 0.0, 1.0) * maxResidualBoost;
  }
};

inline constexpr Config kDefaultConfig{};

struct State {
  double boost = 0.0;
  double dailyDecay = 0.0;
  std::int32_t daysLeft = 0;

  void trigger(double newBoost, const Config &cfg) noexcept {
    const double effective = std::max(0.0, newBoost);
    if (effective <= 0.0) {
      return;
    }
    // Refresh to the stronger of current/incoming; don't stack
    // multiple deposits into an unbounded multiplier.
    boost = std::max(boost, effective);
    dailyDecay = boost / static_cast<double>(cfg.activeDays);
    daysLeft = cfg.activeDays;
  }

  double advance() noexcept {
    if (daysLeft <= 0 || boost <= 0.0) {
      reset();
      return 1.0;
    }
    const double multiplier = 1.0 + boost;
    --daysLeft;
    boost = std::max(0.0, boost - dailyDecay);
    if (daysLeft <= 0) {
      reset();
    }
    return multiplier;
  }

private:
  void reset() noexcept {
    boost = 0.0;
    dailyDecay = 0.0;
    daysLeft = 0;
  }
};

} // namespace PhantomLedger::math::paycheck
