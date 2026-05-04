#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/spending/market/market.hpp"
#include "phantomledger/spending/obligations/snapshot.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <vector>

namespace PhantomLedger::spending::simulator {

[[nodiscard]] std::vector<transactions::Transaction>
simulate(market::Market &market, random::Rng &rng,
         const transactions::Factory &factory,
         const obligations::Snapshot &obligations);

} // namespace PhantomLedger::spending::simulator
