#include "phantomledger/transfers/legit/ledger/limits.hpp"

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/cards.hpp"
#include "phantomledger/entities/synth/personas/pack.hpp"
#include "phantomledger/transactions/clearing/balance_book.hpp"
#include "phantomledger/transfers/legit/ledger/burdens.hpp"

#include <stdexcept>
#include <unordered_set>

namespace PhantomLedger::transfers::legit::ledger {

namespace {

[[nodiscard]] bool
hasCreditCards(const entity::card::Registry *cards) noexcept {
  return cards != nullptr && !cards->records.empty();
}

[[nodiscard]] std::unordered_set<clearing::Ledger::Index>
hubIndicesFromKeys(const blueprints::LegitBuildPlan &plan,
                   const entity::account::Lookup &lookup) {
  std::unordered_set<clearing::Ledger::Index> out;
  out.reserve(plan.counterparties.hubAccounts.size());

  for (const auto &key : plan.counterparties.hubAccounts) {
    const auto it = lookup.byId.find(key);
    if (it != lookup.byId.end()) {
      out.insert(static_cast<clearing::Ledger::Index>(it->second));
    }
  }
  return out;
}

void applyCreditCardLimits(clearing::Ledger &ledger,
                           const entity::card::Registry &cards,
                           const entity::account::Lookup &lookup) {
  for (const auto &card : cards.records) {
    const auto it = lookup.byId.find(card.key);
    if (it == lookup.byId.end()) {
      continue;
    }
    const auto idx = static_cast<clearing::Ledger::Index>(it->second);
    ledger.setOverdraftOnly(idx, card.creditLimit);
  }
}

} // namespace

std::unique_ptr<clearing::Ledger>
buildBalanceBook(const BalanceBookRequest &request,
                 const blueprints::LegitBuildPlan &plan) {
  if (request.rules == nullptr) {
    return nullptr;
  }
  if (!request.rules->enableConstraints) {
    return nullptr;
  }

  if (request.accounts == nullptr || request.accountsLookup == nullptr ||
      request.ownership == nullptr) {
    throw std::invalid_argument(
        "buildBalanceBook requires accounts, accountsLookup and ownership");
  }
  if (request.rng == nullptr) {
    throw std::invalid_argument("buildBalanceBook requires a non-null rng");
  }
  if (plan.personas.pack == nullptr) {
    throw std::invalid_argument(
        "buildBalanceBook requires a populated PersonaPlan.pack");
  }

  auto ledger = std::make_unique<clearing::Ledger>();
  const auto count =
      static_cast<clearing::Ledger::Index>(request.accounts->records.size());
  ledger->initialize(count);

  for (clearing::Ledger::Index idx = 0; idx < count; ++idx) {
    ledger->addAccount(request.accounts->records[idx].id, idx);
  }

  // Hub indices set — required by bootstrap() to assign infinite cash.
  const auto hubIndices = hubIndicesFromKeys(plan, *request.accountsLookup);

  clearing::bootstrap(*ledger, *request.rng, *request.accounts,
                      *request.ownership, plan.personas.pack->table, hubIndices,
                      *request.rules);

  for (const auto idx : hubIndices) {
    ledger->createHub(idx);
  }

  // Burden buffers — 35% of the per-person monthly fixed obligation
  // total. Built from the obligations stream + insurance ledger; see
  // burdens.hpp for the windowing rationale.
  if (request.portfolios != nullptr) {
    const auto personCount = static_cast<std::uint32_t>(plan.persons.size());
    const auto monthlyBurdens =
        buildMonthlyBurdens(*request.portfolios, personCount, plan.startDate);
    clearing::addBurdenBuffer(*ledger, *request.ownership, monthlyBurdens,
                              personCount);
  }

  // Credit-card limits, applied last so they don't interact with
  // protection sampling above.
  if (hasCreditCards(request.creditCards)) {
    applyCreditCardLimits(*ledger, *request.creditCards,
                          *request.accountsLookup);
  }

  return ledger;
}

} // namespace PhantomLedger::transfers::legit::ledger
