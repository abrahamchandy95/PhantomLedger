#include "phantomledger/transfers/legit/routines/credit_cards.hpp"

#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/transfers/credit_cards/lifecycle.hpp"

#include <stdexcept>
#include <unordered_map>

namespace PhantomLedger::transfers::legit::routines::credit_cards {

namespace {

[[nodiscard]] time::Window
windowFromPlan(const blueprints::LegitBuildPlan &plan) noexcept {
  return time::Window{
      .start = plan.startDate,
      .days = plan.days,
  };
}

[[nodiscard]] std::unordered_map<entity::PersonId, entity::Key>
primaryAccountKeysByPerson(const blueprints::LegitBuildPlan &plan) {
  std::unordered_map<entity::PersonId, entity::Key> out;
  if (plan.allAccounts == nullptr) {
    return out;
  }

  out.reserve(plan.primaryAcctRecordIx.size());
  for (const auto &kv : plan.primaryAcctRecordIx) {
    const auto &record = plan.allAccounts->records[kv.second];
    out.emplace(kv.first, record.id);
  }
  return out;
}

} // namespace

std::vector<transactions::Transaction>
generateLifecycle(const LifecycleRunRequest &request,
                  const blueprints::LegitBuildPlan &plan,
                  const transactions::Factory &txf,
                  std::span<const transactions::Transaction> existingTxns) {
  if (request.cards == nullptr || request.cards->records.empty()) {
    return {};
  }
  if (request.rng == nullptr) {
    throw std::invalid_argument("credit_cards routine requires a non-null rng");
  }

  if (request.lifecycle == nullptr) {
    throw std::invalid_argument(
        "credit_cards routine requires lifecycle rules");
  }

  const auto primaryByPerson = primaryAccountKeysByPerson(plan);

  ::PhantomLedger::transfers::credit_cards::LedgerView view{
      .cards = *request.cards,
      .primaryAccounts = primaryByPerson,
      .issuerAccount = plan.counterparties.issuerAcct,
  };

  const random::RngFactory rngFactory{plan.seed};

  ::PhantomLedger::transfers::credit_cards::Lifecycle lifecycle{
      *request.lifecycle,
      txf,
      rngFactory,
      view,
  };

  return lifecycle.generate(windowFromPlan(plan), existingTxns);
}

} // namespace PhantomLedger::transfers::legit::routines::credit_cards
