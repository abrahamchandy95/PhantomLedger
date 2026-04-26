#pragma once

#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/spending/actors/spender.hpp"
#include "phantomledger/spending/market/market.hpp"

#include <cstdint>

namespace PhantomLedger::spending::spenders {

[[nodiscard]] double totalTargetTxns(double txnsPerMonth,
                                     std::uint32_t activeSpenders,
                                     std::uint32_t days) noexcept;

[[nodiscard]] double baseRateForTarget(const actors::Spender &spender,
                                       double dayShock,
                                       time::TimePoint dayStart,
                                       double targetRealizedPerDay,
                                       double dynamicsMultiplier,
                                       double liquidityMultiplier) noexcept;

} // namespace PhantomLedger::spending::spenders
