#pragma once

#include "phantomledger/spending/market/market.hpp"
#include "phantomledger/spending/obligations/snapshot.hpp"
#include "phantomledger/spending/simulator/config.hpp"
#include "phantomledger/spending/simulator/engine.hpp"
#include "phantomledger/transactions/record.hpp"

#include <vector>

namespace PhantomLedger::spending::simulator {

[[nodiscard]] std::vector<transactions::Transaction>
simulate(const market::Market &market, Engine &engine,
         const obligations::Snapshot &obligations,
         const SimulatorConfig &config);

} // namespace PhantomLedger::spending::simulator
