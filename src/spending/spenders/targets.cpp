#include "phantomledger/spending/spenders/targets.hpp"

#include "phantomledger/math/counts.hpp"
#include "phantomledger/spending/liquidity/factor.hpp"

#include <algorithm>

namespace PhantomLedger::spending::spenders {

double totalTargetTxns(double txnsPerMonth, std::uint32_t activeSpenders,
                       std::uint32_t days) noexcept {
  // Mirrors Python: txns_per_month * spenders * (days / 30).
  return txnsPerMonth * static_cast<double>(activeSpenders) *
         (static_cast<double>(days) / 30.0);
}

double baseRateForTarget(const actors::Spender &spender, double dayShock,
                         time::TimePoint dayStart, double targetRealizedPerDay,
                         double dynamicsMultiplier,
                         double liquidityMultiplier) noexcept {
  if (targetRealizedPerDay <= 0.0) {
    return 0.0;
  }

  const double suppression = spender.persona->cash.rateMultiplier *
                             math::counts::weekdayMultiplier(dayStart) *
                             dayShock * std::max(0.0, dynamicsMultiplier) *
                             liquidity::countFactor(liquidityMultiplier);

  if (suppression <= 0.0) {
    return 0.0;
  }
  return targetRealizedPerDay / suppression;
}

} // namespace PhantomLedger::spending::spenders
