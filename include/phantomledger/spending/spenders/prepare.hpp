#pragma once

#include "phantomledger/spending/market/market.hpp"
#include "phantomledger/spending/obligations/snapshot.hpp"
#include "phantomledger/spending/spenders/prepared.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"

#include <vector>

namespace PhantomLedger::spending::spenders {

[[nodiscard]] std::vector<PreparedSpender>
prepareSpenders(const market::Market &market,
                const obligations::Snapshot &obligations,
                const clearing::Ledger *ledger);

} // namespace PhantomLedger::spending::spenders
