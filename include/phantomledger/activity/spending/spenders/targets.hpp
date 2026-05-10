#pragma once

#include "phantomledger/activity/spending/actors/counts.hpp"
#include "phantomledger/activity/spending/actors/spender.hpp"

#include <cstdint>

namespace PhantomLedger::spending::spenders {

[[nodiscard]] double totalTargetTxns(double txnsPerMonth,
                                     std::uint32_t activeSpenders,
                                     std::uint32_t days) noexcept;

[[nodiscard]] double baseRateForTarget(const actors::Spender &spender,
                                       const actors::RatePieces &ratePieces,
                                       double targetRealizedPerDay) noexcept;

} // namespace PhantomLedger::spending::spenders
