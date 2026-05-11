#include "phantomledger/activity/spending/spenders/targets.hpp"

namespace PhantomLedger::spending::spenders {

double totalTargetTxns(double txnsPerMonth, std::uint32_t activeSpenders,
                       std::uint32_t days) noexcept {
  return txnsPerMonth * static_cast<double>(activeSpenders) *
         (static_cast<double>(days) / 30.0);
}

} // namespace PhantomLedger::spending::spenders
