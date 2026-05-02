#pragma once

#include "phantomledger/spending/market/market.hpp"
#include "phantomledger/spending/obligations/snapshot.hpp"
#include "phantomledger/spending/simulator/day_driver.hpp"
#include "phantomledger/spending/simulator/engine.hpp"
#include "phantomledger/spending/simulator/run_planner.hpp"
#include "phantomledger/transactions/record.hpp"

#include <vector>

namespace PhantomLedger::spending::simulator {

[[nodiscard]] std::vector<transactions::Transaction>
simulate(market::Market &market, Engine &engine,
         const obligations::Snapshot &obligations, RunPlanner planner = {},
         DayDriver dayDriver = DayDriver{});

} // namespace PhantomLedger::spending::simulator
