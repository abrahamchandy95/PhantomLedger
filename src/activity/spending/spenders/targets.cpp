#include "phantomledger/activity/spending/spenders/targets.hpp"

namespace PhantomLedger::spending::spenders {

double totalTargetTxns(double txnsPerMonth, std::uint32_t activeSpenders,
                       std::uint32_t days) noexcept {
  return txnsPerMonth * static_cast<double>(activeSpenders) *
         (static_cast<double>(days) / 30.0);
}

double baseRateForTarget(const actors::Spender &spender,
                         const actors::RatePieces &ratePieces,
                         double targetRealizedPerDay) noexcept {
  if (targetRealizedPerDay <= 0.0) {
    return 0.0;
  }

  const double suppression = ratePieces.suppression(spender);

  if (suppression <= 0.0) {
    return 0.0;
  }
  return targetRealizedPerDay / suppression;
}

} // namespace PhantomLedger::spending::spenders
