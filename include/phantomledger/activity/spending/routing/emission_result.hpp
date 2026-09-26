#pragma once

#include "phantomledger/synth/counterparties/remote_payees.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/draft.hpp"

#include <span>

namespace PhantomLedger::activity::spending::routing {

struct EmissionResult {
  transactions::Draft draft;
  clearing::Ledger::Index srcIdx = clearing::Ledger::invalid;
  clearing::Ledger::Index dstIdx = clearing::Ledger::invalid;
};

struct ResolvedAccounts {
  std::span<const clearing::Ledger::Index> personPrimaryIdx;

  std::span<const clearing::Ledger::Index> merchantCounterpartyIdx;

  // unknown-counterparty-2026-09: the identified remote merchants the
  // external-unknown slot pays when a row is not a paid check, and that
  // slot's probability mass (the denominator of the check share). Every
  // destination of the retired catch-all flows is external, so none needs a
  // ledger index.
  const ::PhantomLedger::synth::counterparties::remote::RemoteMerchantTable
      *remoteMerchants = nullptr;
  double unattributedSlotShare = 0.0;
};

} // namespace PhantomLedger::activity::spending::routing
