#pragma once

#include "phantomledger/entities/counterparties/sized_pool.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"

#include <memory>
#include <span>
#include <vector>

namespace PhantomLedger::transfers::legit::ledger {

struct LegitTxnStreams {
  std::vector<transactions::Transaction> replaySortedTxns;
};

struct LegitOpeningBook {
  std::unique_ptr<clearing::Ledger> initialBook;

  [[nodiscard]] bool hasInitialBook() const noexcept {
    return initialBook != nullptr;
  }
};

struct LegitCounterparties {
  std::vector<entity::Key> billerAccounts;
  // The payroll pool WITH its size law (counterparty-sizes-2026-09):
  // camouflage salary must pick from the same law legitimate payroll does.
  // Every carrier (the monolith, the windowed engine and both harnesses)
  // copies it from the blueprint.
  entity::counterparty::SizedKeys employers;

  [[nodiscard]] std::span<const entity::Key> billerView() const noexcept {
    return std::span<const entity::Key>(billerAccounts.data(),
                                        billerAccounts.size());
  }
};

struct LegitTransferResult {
  LegitTxnStreams txns;
  LegitOpeningBook openingBook;
  LegitCounterparties counterparties;
};

} // namespace PhantomLedger::transfers::legit::ledger
