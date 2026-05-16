#pragma once

#include <cstdint>

namespace PhantomLedger::activity::spending::spenders {

[[nodiscard]] double totalTargetTxns(double txnsPerMonth,
                                     std::uint32_t activeSpenders,
                                     std::uint32_t days) noexcept;

} // namespace PhantomLedger::activity::spending::spenders
