#pragma once

#include "phantomledger/activity/spending/obligations/burden.hpp"
#include "phantomledger/transactions/record.hpp"

#include <span>

namespace PhantomLedger::activity::spending::obligations {

struct Snapshot {
  std::span<const transactions::Transaction> baseTxns;
  bool baseTxnsSorted = false;
  Burden burden;
};

} // namespace PhantomLedger::activity::spending::obligations
