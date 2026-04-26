#pragma once

#include "phantomledger/spending/obligations/burden.hpp"
#include "phantomledger/transactions/record.hpp"

#include <span>

namespace PhantomLedger::spending::obligations {

struct Snapshot {
  std::span<const transactions::Transaction> baseTxns;
  bool baseTxnsSorted = false;
  Burden burden;
};

} // namespace PhantomLedger::spending::obligations
