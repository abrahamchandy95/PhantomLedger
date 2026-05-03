#pragma once

#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/spending/liquidity/snapshot.hpp"

#include <algorithm>
#include <cstdint>

namespace PhantomLedger::spending::liquidity {

struct Throttle {
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

inline constexpr double kCeiling = 1.10;

[[nodiscard]] inline double multiplier(const Throttle &throttle,
                                       const Snapshot &snap) noexcept {
  if (!throttle.enabled) {
    return 1.0;
  }

  double relief = 0.0;
  if (throttle.reliefDays > 0 && snap.daysSincePayday <= throttle.reliefDays) {
    relief = static_cast<double>(throttle.reliefDays - snap.daysSincePayday) /
             static_cast<double>(throttle.reliefDays);
  }

  const auto stressDays = snap.daysSincePayday > throttle.stressStartDay
                              ? snap.daysSincePayday - throttle.stressStartDay
                              : std::uint32_t{0};
  const double stress =
      std::min(1.0, static_cast<double>(stressDays) /
                        static_cast<double>(std::max<std::uint32_t>(
                            1u, throttle.stressRampDays)));

  // Sensitivity-weighted up/down legs.
  constexpr double kStressDownBase = 0.35;
  constexpr double kStressDownScale = 0.95;
  constexpr double kReliefUpBase = 0.06;
  constexpr double kReliefUpScale = 0.10;

  const double cycleDown =
      stress * (kStressDownBase + kStressDownScale * snap.paycheckSensitivity);
  const double cycleUp =
      relief * (kReliefUpBase + kReliefUpScale * snap.paycheckSensitivity);

  constexpr double kCycleCeiling = 1.05;
  double cycleMult = 1.0 - cycleDown + cycleUp;
  cycleMult = std::clamp(cycleMult, throttle.absoluteFloor, kCycleCeiling);

  // ---- Cash-on-hand ratio ------------------------------------------
  constexpr double kCashRefFloor =
      150.0; // matches PreparedSpender baselineCash floor
  const double cashRef = std::max(kCashRefFloor, snap.baselineCash);
  const double cashRatio = std::clamp(snap.availableCash / cashRef, 0.0, 2.0);

  constexpr double kCashOffset = 0.10;
  constexpr double kCashCeiling = 1.10;
  const double cashMult =
      std::clamp(kCashOffset + cashRatio, 0.0, kCashCeiling);

  // ---- Fixed-burden penalty ----------------------------------------
  const double burdenRatio =
      std::clamp(snap.fixedMonthlyBurden / cashRef, 0.0, 2.0);

  constexpr double kBurdenSlope = 0.35;
  constexpr double kBurdenFloor = 0.30;
  const double burdenMult =
      std::max(kBurdenFloor, 1.0 - kBurdenSlope * burdenRatio);

  // ---- Compose & final clip ----------------------------------------
  const double mult = cycleMult * cashMult * burdenMult;
  return std::clamp(mult, 0.0, kCeiling);
}

} // namespace PhantomLedger::spending::liquidity
