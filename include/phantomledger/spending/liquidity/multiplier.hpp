#pragma once

#include "phantomledger/spending/config/liquidity.hpp"
#include "phantomledger/spending/liquidity/snapshot.hpp"

#include <algorithm>

namespace PhantomLedger::spending::liquidity {

[[nodiscard]] inline double
multiplier(const config::LiquidityConstraints &policy,
           const Snapshot &snap) noexcept {
  if (!policy.enabled) {
    return 1.0;
  }

  double relief = 0.0;
  if (policy.reliefDays > 0 && snap.daysSincePayday <= policy.reliefDays) {
    relief = static_cast<double>(policy.reliefDays - snap.daysSincePayday) /
             static_cast<double>(policy.reliefDays);
  }

  const auto stressDays = snap.daysSincePayday > policy.stressStartDay
                              ? snap.daysSincePayday - policy.stressStartDay
                              : std::uint32_t{0};
  const double stress =
      std::min(1.0, static_cast<double>(stressDays) /
                        static_cast<double>(std::max<std::uint32_t>(
                            1u, policy.stressRampDays)));

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
  cycleMult = std::clamp(cycleMult, policy.absoluteFloor, kCycleCeiling);

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
  return std::clamp(mult, 0.0, config::kLiquidityCeiling);
}

} // namespace PhantomLedger::spending::liquidity
