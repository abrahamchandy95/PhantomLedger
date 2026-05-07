#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"

#include <span>
#include <unordered_set>

namespace PhantomLedger::transfers::subscriptions {

struct Population {
  std::span<const entity::Key> primaryAccountByPerson;

  const std::unordered_set<entity::Key, std::hash<entity::Key>> *hubSet =
      nullptr;
};

struct Counterparties {
  std::span<const entity::Key> billerAccounts;
};
struct Screen {
  clearing::Ledger *ledger = nullptr;
  std::span<const transactions::Transaction> baseTxns;
};

} // namespace PhantomLedger::transfers::subscriptions
