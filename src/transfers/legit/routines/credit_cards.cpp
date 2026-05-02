#include "phantomledger/transfers/legit/routines/credit_cards.hpp"

#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/transfers/credit_cards/lifecycle.hpp"

#include <stdexcept>
#include <unordered_map>

namespace PhantomLedger::transfers::legit::routines::credit_cards {

namespace {

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
generateLifecycle(const blueprints::Blueprint &request,
                  const blueprints::LegitBuildPlan &plan,
                  const transactions::Factory &txf,
                  std::span<const transactions::Transaction> existingTxns) {
  if (!request.creditCardState.enabled() ||
      request.creditCardState.cards == nullptr) {
    return {};
  }
  if (request.timeline.rng == nullptr) {
    throw std::invalid_argument(
        "credit_cards routine requires a non-null timeline.rng");
  }

  const auto *issuerPolicy = request.creditCards.terms;
  const auto *behavior = request.creditCards.habits;

  if (issuerPolicy == nullptr || behavior == nullptr) {
    throw std::invalid_argument(
        "credit_cards routine requires terms and habits in Specifications");
  }

  const auto primaryByPerson = primaryAccountKeysByPerson(plan);

  ::PhantomLedger::transfers::credit_cards::LedgerView view{
      .cards = *request.creditCardState.cards,
      .primaryAccounts = primaryByPerson,
      .issuerAccount = plan.counterparties.issuerAcct,
  };

  const random::RngFactory rngFactory{plan.seed};

  ::PhantomLedger::transfers::credit_cards::Lifecycle lifecycle{
      *issuerPolicy, *behavior, txf, rngFactory, view,
  };

  return lifecycle.generate(request.timeline.window, existingTxns);
}

} // namespace PhantomLedger::transfers::legit::routines::credit_cards
