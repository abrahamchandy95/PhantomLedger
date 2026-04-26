#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/factory.hpp"

namespace PhantomLedger::spending::simulator {

struct Engine {
  random::Rng *rng = nullptr;
  const transactions::Factory *factory = nullptr;
  clearing::Ledger *ledger = nullptr;
};

} // namespace PhantomLedger::spending::simulator
