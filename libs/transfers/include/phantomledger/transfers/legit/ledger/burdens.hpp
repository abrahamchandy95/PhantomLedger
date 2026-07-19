#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/products/event.hpp"
#include "phantomledger/entities/products/portfolio.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::transfers::legit::ledger {

inline constexpr int kBurdenWindowMonths = 3;

[[nodiscard]] std::vector<double>
buildMonthlyBurdens(const entity::product::PortfolioRegistry &registry,
                    std::uint32_t personCount, time::TimePoint windowStart);

} // namespace PhantomLedger::transfers::legit::ledger
