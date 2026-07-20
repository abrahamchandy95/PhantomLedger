#pragma once

#include "phantomledger/activity/spending/obligations/burden.hpp"
#include "phantomledger/transactions/record.hpp"

#include <span>

namespace PhantomLedger::activity::spending {
class BaseReplaySource;
} // namespace PhantomLedger::activity::spending

namespace PhantomLedger::activity::spending::obligations {

struct Snapshot {
  std::span<const transactions::Transaction> baseTxns;
  bool baseTxnsSorted = false;
  Burden burden;

  // RAM R2.4b (replay_source.hpp): when set, the day driver's ledger
  // replay feeds from this source instead of the resident baseTxns
  // span — same rows, same order, non-resident backing (the disk-spool
  // seam). Null selects the span adapter.
  const BaseReplaySource *baseReplayOverride = nullptr;
};

} // namespace PhantomLedger::activity::spending::obligations
