#pragma once

#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/blueprints/models.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"

#include <span>
#include <vector>

namespace PhantomLedger::transfers::legit::routines::credit_cards {

[[nodiscard]] std::vector<transactions::Transaction>
generateLifecycle(const blueprints::Blueprint &request,
                  const blueprints::LegitBuildPlan &plan,
                  const transactions::Factory &txf,
                  std::span<const transactions::Transaction> existingTxns);

} // namespace PhantomLedger::transfers::legit::routines::credit_cards
