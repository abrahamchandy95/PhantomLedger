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
hubIndicesFromKeys(const blueprints::LegitBlueprint &plan,
                   const entity::account::Lookup &lookup) {
  std::unordered_set<clearing::Ledger::Index> out;
  out.reserve(plan.counterparties().hubAccounts.size());

  for (const auto &key : plan.counterparties().hubAccounts) {
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

OpeningBook::OpeningBook(random::Rng &rng, Accounts accounts,
                         Protections protections) noexcept
    : rng_{&rng}, accounts_{accounts}, protections_{protections} {}

OpeningBook &OpeningBook::rng(random::Rng &value) noexcept {
  rng_ = &value;
  return *this;
}

OpeningBook &OpeningBook::accounts(Accounts value) noexcept {
  accounts_ = value;
  return *this;
}

OpeningBook &OpeningBook::protections(Protections value) noexcept {
  protections_ = value;
  return *this;
}

std::unique_ptr<clearing::Ledger>
OpeningBook::build(const blueprints::LegitBlueprint &plan) const {
  if (protections_.balanceRules == nullptr) {
    return nullptr;
  }
  if (!protections_.balanceRules->enableConstraints) {
    return nullptr;
  }

  if (accounts_.registry == nullptr || accounts_.lookup == nullptr ||
      accounts_.ownership == nullptr) {
    throw std::invalid_argument(
        "OpeningBook requires accounts, lookup and ownership");
  }
  if (rng_ == nullptr) {
    throw std::invalid_argument("OpeningBook requires a non-null rng");
  }
  if (plan.personas().pack == nullptr) {
    throw std::invalid_argument(
        "OpeningBook requires a populated PersonaPlan.pack");
  }

  auto ledger = std::make_unique<clearing::Ledger>();
  const auto count =
      static_cast<clearing::Ledger::Index>(accounts_.registry->records.size());
  ledger->initialize(count);

  for (clearing::Ledger::Index idx = 0; idx < count; ++idx) {
    ledger->addAccount(accounts_.registry->records[idx].id, idx);
  }

  // Hub indices set — required by bootstrap() to assign infinite cash.
  const auto hubIndices = hubIndicesFromKeys(plan, *accounts_.lookup);

  clearing::bootstrap(*ledger, *rng_, *accounts_.registry, *accounts_.ownership,
                      plan.personas().pack->table, hubIndices,
                      *protections_.balanceRules);

  for (const auto idx : hubIndices) {
    ledger->createHub(idx);
  }

  // Burden buffers — 35% of the per-person monthly fixed obligation
  // total. Built from the obligations stream + insurance ledger; see
  // burdens.hpp for the windowing rationale.
  if (protections_.portfolios != nullptr) {
    const auto personCount = static_cast<std::uint32_t>(plan.persons().size());
    const auto monthlyBurdens = buildMonthlyBurdens(
        *protections_.portfolios, personCount, plan.startDate());
    clearing::addBurdenBuffer(*ledger, *accounts_.ownership, monthlyBurdens,
                              personCount);
  }

  // Credit-card limits, applied last so they don't interact with
  // protection sampling above.
  if (hasCreditCards(protections_.creditCards)) {
    applyCreditCardLimits(*ledger, *protections_.creditCards,
                          *accounts_.lookup);
  }

  return ledger;
}

} // namespace PhantomLedger::transfers::legit::ledger
