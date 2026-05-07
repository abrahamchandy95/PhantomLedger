#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/subscriptions/bundle.hpp"

#include <span>
#include <unordered_set>
#include <vector>

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

[[nodiscard]] std::vector<transactions::Transaction>
debits(const BundleRules &terms, const time::Window &window, random::Rng &rng,
       const transactions::Factory &txf, const random::RngFactory &factory,
       const Population &population, const Counterparties &counterparties,
       const Screen &screen = {});

} // namespace PhantomLedger::transfers::subscriptions
