#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

#include <algorithm>
#include <cstdint>

namespace PhantomLedger::math::dormancy {

enum class Phase : std::uint8_t {
  active = 0,
  dormant = 1,
  waking = 2,
};

struct Config {
  double enterP = 0.0012;
  int durationMin = 7;
  int durationMax = 45;
  int wakeDaysMin = 2;
  int wakeDaysMax = 5;
  double dormantRate = 0.05;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::between("enterP", enterP, 0.0, 0.1); });
    r.check([&] { v::ge("durationMin", durationMin, 1); });
    r.check([&] { v::ge("durationMax", durationMax, durationMin); });
    r.check([&] { v::ge("wakeDaysMin", wakeDaysMin, 1); });
    r.check([&] { v::ge("wakeDaysMax", wakeDaysMax, wakeDaysMin); });
    r.check([&] { v::unit("dormantRate", dormantRate); });
  }
};

inline constexpr Config kDefaultConfig{};

struct State {
  Phase phase = Phase::active;
  std::uint8_t remaining = 0;
  std::uint8_t wakeTotal = 0;
  std::uint8_t _pad = 0;

  [[nodiscard]] constexpr double
  rateMultiplier(const Config &cfg) const noexcept {
    switch (phase) {
    case Phase::active:
      return 1.0;
    case Phase::dormant:
      return cfg.dormantRate;
    case Phase::waking: {
      if (wakeTotal == 0) {
        return 1.0;
      }
      const double progress =
          1.0 - static_cast<double>(remaining) / static_cast<double>(wakeTotal);
      const double clamped = std::clamp(progress, 0.0, 1.0);
      return cfg.dormantRate + clamped * (1.0 - cfg.dormantRate);
    }
    }
    return 1.0;
  }

  double advance(random::Rng &rng, const Config &cfg) {
    const double multiplier = rateMultiplier(cfg);

    switch (phase) {
    case Phase::active:
      if (rng.coin(cfg.enterP)) {
        phase = Phase::dormant;
        remaining = static_cast<std::uint8_t>(
            rng.uniformInt(cfg.durationMin, cfg.durationMax + 1));
      }
      break;

    case Phase::dormant:
      if (remaining > 0) {
        --remaining;
      }
      if (remaining == 0) {
        phase = Phase::waking;
        wakeTotal = static_cast<std::uint8_t>(
            rng.uniformInt(cfg.wakeDaysMin, cfg.wakeDaysMax + 1));
        remaining = wakeTotal;
      }
      break;

    case Phase::waking:
      if (remaining > 0) {
        --remaining;
      }
      if (remaining == 0) {
        phase = Phase::active;
        wakeTotal = 0;
      }
      break;
    }
    return multiplier;
  }
};

static_assert(sizeof(State) == 4, "dormancy::State should pack into 4 bytes");

} // namespace PhantomLedger::math::dormancy
