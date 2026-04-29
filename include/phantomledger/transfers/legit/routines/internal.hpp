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

namespace PhantomLedger::transfers::legit::routines::internal_xfer {

struct Config {
  double activeP = 0.45;

  std::int32_t transfersPerMonthMin = 1;
  std::int32_t transfersPerMonthMax = 3;

  double amountMedian = 250.0;
  double amountSigma = 0.8;

  double roundAmountP = 0.45;

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

} // namespace PhantomLedger::transfers::legit::routines::internal_xfer
