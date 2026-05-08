#pragma once

#include "phantomledger/activity/spending/market/market.hpp"
#include "phantomledger/activity/spending/obligations/snapshot.hpp"
#include "phantomledger/activity/spending/spenders/prepared.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"

#include <vector>

namespace PhantomLedger::spending::spenders {

[[nodiscard]] std::vector<PreparedSpender>
prepareSpenders(const market::Market &market,
                const obligations::Snapshot &obligations,
                const clearing::Ledger *ledger);

} // namespace PhantomLedger::spending::spenders
