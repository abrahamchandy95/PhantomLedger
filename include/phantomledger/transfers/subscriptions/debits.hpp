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

// Read-only view of the population.
struct Population {
  std::span<const entity::Key> primaryAccountByPerson;

  // Optional hub-account set (e.g. ATM hub).
  const std::unordered_set<entity::Key, std::hash<entity::Key>> *hubSet =
      nullptr;
};

// External counterparties that subscriptions debit *to*.
struct Counterparties {
  std::span<const entity::Key> billerAccounts;
};

// Optional screening surface.
struct Screen {
  clearing::Ledger *ledger = nullptr;
  std::span<const transactions::Transaction> baseTxns;
};

// Generate sorted subscription debit transactions for the window.
[[nodiscard]] std::vector<transactions::Transaction>
debits(const BundleTerms &terms, const time::Window &window, random::Rng &rng,
       const transactions::Factory &txf, const random::RngFactory &factory,
       const Population &population, const Counterparties &counterparties,
       const Screen &screen = {});

} // namespace PhantomLedger::transfers::subscriptions
