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

namespace PhantomLedger::transfers::legit::routines::atm {

struct Config {
  double userP = 0.88;
  std::int32_t withdrawalsPerMonthMin = 1;
  std::int32_t withdrawalsPerMonthMax = 6;

  void validate() const;
};

inline constexpr Config kDefaultConfig{};

[[nodiscard]] std::vector<transactions::Transaction>
generate(random::Rng &rng, const blueprints::LegitBuildPlan &plan,
         const transactions::Factory &txf,
         const entity::account::Ownership &ownership,
         const entity::account::Registry &registry,
         clearing::Ledger *book = nullptr,
         std::span<const transactions::Transaction> baseTxns = {},
         bool baseTxnsSorted = false, Config cfg = kDefaultConfig);

} // namespace PhantomLedger::transfers::legit::routines::atm
