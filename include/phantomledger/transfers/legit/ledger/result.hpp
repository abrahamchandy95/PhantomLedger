#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"

#include <memory>
#include <vector>

namespace PhantomLedger::transfers::legit::ledger {

struct TransfersPayload {
  std::vector<transactions::Transaction> candidateTxns;
  std::vector<entity::Key> hubAccounts;
  std::vector<entity::Key> billerAccounts;
  std::vector<entity::Key> employers;
  std::unique_ptr<clearing::Ledger> initialBook;
  std::vector<transactions::Transaction> replaySortedTxns;
};

} // namespace PhantomLedger::transfers::legit::ledger
