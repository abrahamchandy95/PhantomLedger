#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"

#include <span>
#include <vector>

namespace PhantomLedger::transfers::legit::routines::paychecks {

[[nodiscard]] std::vector<transactions::Transaction>
splitDeposits(random::Rng &rng, const blueprints::LegitBuildPlan &plan,
              const transactions::Factory &txf,
              const entity::account::Ownership &ownership,
              const entity::account::Registry &registry,
              std::span<const transactions::Transaction> existingTxns);

} // namespace PhantomLedger::transfers::legit::routines::paychecks
