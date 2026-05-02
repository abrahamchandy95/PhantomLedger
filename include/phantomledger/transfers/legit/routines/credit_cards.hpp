#pragma once

#include "phantomledger/entities/cards.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"

#include <span>
#include <vector>

namespace PhantomLedger::transfers::credit_cards {
struct LifecycleRules;
} // namespace PhantomLedger::transfers::credit_cards

namespace PhantomLedger::transfers::legit::routines::credit_cards {

struct LifecycleRunRequest {
  random::Rng *rng = nullptr;
  const entity::card::Registry *cards = nullptr;
  const ::PhantomLedger::transfers::credit_cards::LifecycleRules *lifecycle =
      nullptr;
};

[[nodiscard]] std::vector<transactions::Transaction>
generateLifecycle(const LifecycleRunRequest &request,
                  const blueprints::LegitBuildPlan &plan,
                  const transactions::Factory &txf,
                  std::span<const transactions::Transaction> existingTxns);

} // namespace PhantomLedger::transfers::legit::routines::credit_cards
