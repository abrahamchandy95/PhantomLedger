#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"

#include <memory>
#include <span>
#include <vector>

namespace PhantomLedger::transfers::legit::ledger {

struct LegitTxnStreams {
  std::vector<transactions::Transaction> candidateTxns;
  std::vector<transactions::Transaction> replaySortedTxns;
};

struct LegitOpeningBook {
  std::unique_ptr<clearing::Ledger> initialBook;

  [[nodiscard]] bool hasInitialBook() const noexcept {
    return initialBook != nullptr;
  }
};

struct LegitCounterparties {
  std::vector<entity::Key> hubAccounts;
  std::vector<entity::Key> billerAccounts;
  std::vector<entity::Key> employers;

  [[nodiscard]] std::span<const entity::Key> billerView() const noexcept {
    return std::span<const entity::Key>(billerAccounts.data(),
                                        billerAccounts.size());
  }

  [[nodiscard]] std::span<const entity::Key> employerView() const noexcept {
    return std::span<const entity::Key>(employers.data(), employers.size());
  }
};

struct LegitTransferResult {
  LegitTxnStreams txns;
  LegitOpeningBook openingBook;
  LegitCounterparties counterparties;
};

} // namespace PhantomLedger::transfers::legit::ledger
