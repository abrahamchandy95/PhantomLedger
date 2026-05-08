#pragma once

#include "phantomledger/primitives/time/calendar.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::transfers::credit_cards {

[[nodiscard]] std::vector<time::TimePoint>
statementCloseDates(time::TimePoint windowStart, time::TimePoint windowEndExcl,
                    std::uint8_t cycleDay);

[[nodiscard]] time::TimePoint resolveDueDate(time::TimePoint due) noexcept;

[[nodiscard]] bool isOnTime(time::TimePoint paymentTimestamp,
                            time::TimePoint due) noexcept;

} // namespace PhantomLedger::transfers::credit_cards
