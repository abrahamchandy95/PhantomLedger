#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::transfers::legit::routines::subscriptions {

struct Config {
  std::int32_t minPerPerson = 4;
  std::int32_t maxPerPerson = 8;
  double debitP = 0.55;
  std::int32_t dayJitter = 1;

  void validate() const;
};

inline constexpr Config kDefaultConfig{};

[[nodiscard]] std::vector<transactions::Transaction>
generate(random::Rng &rng, const blueprints::LegitBuildPlan &plan,
         const transactions::Factory &txf,
         const entity::account::Registry &registry,
         clearing::Ledger *book = nullptr,
         std::span<const transactions::Transaction> baseTxns = {},
         bool baseTxnsSorted = false, Config cfg = kDefaultConfig);

} // namespace PhantomLedger::transfers::legit::routines::subscriptions
