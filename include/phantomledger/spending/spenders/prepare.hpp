#pragma once

#include "phantomledger/spending/market/market.hpp"
#include "phantomledger/spending/obligations/snapshot.hpp"
#include "phantomledger/spending/spenders/prepared.hpp"

#include <vector>

namespace PhantomLedger::spending::spenders {

[[nodiscard]] std::vector<PreparedSpender>
prepareSpenders(const market::Market &market,
                const obligations::Snapshot &obligations);

} // namespace PhantomLedger::spending::spenders
