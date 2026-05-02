#pragma once

#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/factory.hpp"

#include <cstdint>

namespace PhantomLedger::spending::simulator {

struct Engine {
  random::Rng *rng = nullptr;
  const transactions::Factory *factory = nullptr;
  clearing::Ledger *ledger = nullptr;

  random::RngFactory *rngFactory = nullptr;
  std::uint32_t threadCount = 1;
};

} // namespace PhantomLedger::spending::simulator
