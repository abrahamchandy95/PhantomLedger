#pragma once

#include <cstdint>

namespace PhantomLedger::spending::spenders {

[[nodiscard]] double totalTargetTxns(double txnsPerMonth,
                                     std::uint32_t activeSpenders,
                                     std::uint32_t days) noexcept;

} // namespace PhantomLedger::spending::spenders
