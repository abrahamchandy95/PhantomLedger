#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/spending/market/market.hpp"
#include "phantomledger/spending/obligations/snapshot.hpp"
#include "phantomledger/spending/simulator/day_driver.hpp"
#include "phantomledger/spending/simulator/run_planner.hpp"
#include "phantomledger/spending/simulator/spender_emission_driver.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <vector>

namespace PhantomLedger::spending::simulator {

[[nodiscard]] std::vector<transactions::Transaction>
simulate(market::Market &market, random::Rng &rng,
         const transactions::Factory &factory,
         const obligations::Snapshot &obligations,
         clearing::Ledger *ledger = nullptr, RunPlanner planner = {},
         DayDriver dayDriver = DayDriver{},
         SpenderEmissionDriver::Threads emissionThreads = {});

} // namespace PhantomLedger::spending::simulator
