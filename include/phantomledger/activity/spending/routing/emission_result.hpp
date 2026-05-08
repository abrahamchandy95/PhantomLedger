#pragma once

#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/draft.hpp"

#include <span>

namespace PhantomLedger::spending::routing {

struct EmissionResult {
  transactions::Draft draft;
  clearing::Ledger::Index srcIdx = clearing::Ledger::invalid;
  clearing::Ledger::Index dstIdx = clearing::Ledger::invalid;
};

struct ResolvedAccounts {
  std::span<const clearing::Ledger::Index> personPrimaryIdx;

  std::span<const clearing::Ledger::Index> merchantCounterpartyIdx;

  clearing::Ledger::Index externalUnknownIdx = clearing::Ledger::invalid;
};

} // namespace PhantomLedger::spending::routing
