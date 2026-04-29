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

std::unique_ptr<clearing::Ledger> buildBalanceBook(
    const blueprints::Timeline &timeline, const blueprints::Network &network,
    const blueprints::Specifications &specs, const blueprints::CCState &ccState,
    const blueprints::LegitBuildPlan &plan) {
  // Mirror Python: `if not balance_rules.enable_constraints: return None`.
  // We also bail when no rules pointer was supplied — there is no
  // sensible default, and silently substituting `BalanceRules{}` would
  // mask a real configuration mistake.
  if (specs.balances == nullptr) {
    return nullptr;
  }
  if (!specs.balances->enableConstraints) {
    return nullptr;
  }

  if (network.accounts == nullptr || network.accountsLookup == nullptr ||
      network.ownership == nullptr) {
    throw std::invalid_argument(
        "buildBalanceBook requires accounts, accountsLookup and ownership");
  }
  if (timeline.rng == nullptr) {
    throw std::invalid_argument(
        "buildBalanceBook requires a non-null timeline.rng");
  }
  if (plan.personas.pack == nullptr) {
    throw std::invalid_argument(
        "buildBalanceBook requires a populated PersonaPlan.pack");
  }

  // Ledger sizing: one slot per registry record (matches Python's
  // len(plan.all_accounts)).
  auto ledger = std::make_unique<clearing::Ledger>();
  const auto count =
      static_cast<clearing::Ledger::Index>(network.accounts->records.size());
  ledger->initialize(count);

  // Register every internal account so transferAt() can find them.
  // External-only accounts (record.owner == invalidPerson) still
  // occupy a slot, but bootstrap() skips them and they aren't
  // hub-eligible. This loop is the C++ equivalent of the Python
  // `account_indices = {acct: idx ...}` map plus the side-effects
  // performed inside `balances_model.initialize`.
  for (clearing::Ledger::Index idx = 0; idx < count; ++idx) {
    ledger->addAccount(network.accounts->records[idx].id, idx);
  }

  // Hub indices set — required by bootstrap() to assign infinite cash.
  const auto hubIndices = hubIndicesFromKeys(plan, *network.accountsLookup);

  // Run the existing C++ bootstrap. This handles persona-aware buffer
  // sampling, bank-tier draws, protection-type assignment and LOC
  // parameters in a single pass — the same work that Python's
  // `balances_model.initialize` performs, minus the string-based
  // persona indirection.
  clearing::bootstrap(*ledger, *timeline.rng, *network.accounts,
                      *network.ownership, plan.personas.pack->table, hubIndices,
                      *specs.balances);

  // Mark hub indices in the ledger flag word. `bootstrap` assigns the
  // hub cash sentinel but does not set the hub flag bit; the transfer
  // path uses `isHub()` to short-circuit funding checks, so the flag
  // must be set explicitly here.
  for (const auto idx : hubIndices) {
    ledger->createHub(idx);
  }

  // Burden buffers — 35% of the per-person monthly fixed obligation
  // total. Built from the obligations stream + insurance ledger; see
  // burdens.hpp for the windowing rationale.
  if (network.portfolios != nullptr) {
    const auto personCount = static_cast<std::uint32_t>(plan.persons.size());
    const auto monthlyBurdens =
        buildMonthlyBurdens(*network.portfolios, personCount, plan.startDate);
    clearing::addBurdenBuffer(*ledger, *network.ownership, monthlyBurdens,
                              personCount);
  }

  // Credit-card limits, applied last so they don't interact with
  // protection sampling above.
  if (ccState.enabled() && ccState.cards != nullptr) {
    applyCreditCardLimits(*ledger, *ccState.cards, *network.accountsLookup);
  }

  return ledger;
}

} // namespace PhantomLedger::transfers::legit::ledger
