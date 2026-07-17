#pragma once

#include "phantomledger/transfers/legit/blueprints/plans.hpp"
#include "phantomledger/transfers/legit/ledger/limits.hpp"
#include "phantomledger/transfers/legit/ledger/passes.hpp"
#include "phantomledger/transfers/legit/ledger/result.hpp"
#include "phantomledger/transfers/legit/routines/relatives.hpp"

#include <memory>
#include <vector>

namespace PhantomLedger::infra {
class Router;
} // namespace PhantomLedger::infra

namespace PhantomLedger::transfers::legit::ledger {

// Windowed-composition prologue: everything build() produces BEFORE
// spending — blueprint, opening book, income and base routines — plus the
// configured pieces the session composition needs. Move-safe by
// construction: every cross-reference targets heap-stable memory (the
// screen borrows *initialBook, the routine pass points at *txf). The
// TxnStreams' screened view backs the session's market and obligations
// snapshots; do not mutate `streams` while a session built over it runs.
struct WindowedPrologue {
  blueprints::LegitBlueprint plan;
  std::unique_ptr<clearing::Ledger> initialBook;
  TxnStreams streams;
  ScreenBook screen;
  std::unique_ptr<const transactions::Factory> txf;
  passes::RoutinePass routinePass;
};

class LegitTransferBuilder {
public:
  using FamilyTransferScenario = ::PhantomLedger::transfers::legit::routines::
      relatives::FamilyTransferScenario;

  LegitTransferBuilder() = default;
  LegitTransferBuilder(random::Rng &rng, blueprints::LegitTimeframe timeframe,
                       blueprints::AccountCensus census,
                       OpeningBook openingBook) noexcept;

  LegitTransferBuilder &rng(random::Rng &value) noexcept;
  LegitTransferBuilder &timeframe(blueprints::LegitTimeframe value) noexcept;
  LegitTransferBuilder &census(blueprints::AccountCensus value) noexcept;
  LegitTransferBuilder &
  counterparties(blueprints::CounterpartyPools value) noexcept;
  LegitTransferBuilder &personas(blueprints::PersonaCatalog value) noexcept;
  LegitTransferBuilder &
  hubSelection(blueprints::HubSelectionRules value) noexcept;
  LegitTransferBuilder &openingBook(OpeningBook value) noexcept;

  LegitTransferBuilder &income(passes::IncomePass value) noexcept;
  LegitTransferBuilder &routines(passes::RoutinePass value) noexcept;
  LegitTransferBuilder &family(passes::FamilyPass value) noexcept;
  LegitTransferBuilder &credit(passes::CreditLifecyclePass value) noexcept;

  LegitTransferBuilder &familyScenario(FamilyTransferScenario value) noexcept;
  LegitTransferBuilder &
  router(const ::PhantomLedger::infra::Router &value) noexcept;
  LegitTransferBuilder &
  router(const ::PhantomLedger::infra::Router *value) noexcept;

  [[nodiscard]] LegitTransferResult build() const;

  // Windowed-mode prologue: the exact generation build() performs before
  // spending — blueprint (counterparties, personas), opening book, income,
  // base routines via addRoutinesWithoutSpending — consuming the shared
  // sequential stream identically. The windowed driver composition takes
  // over from here (session spending, cursor sources, two-phase fold).
  [[nodiscard]] WindowedPrologue buildWindowedPrologue() const;

  // Family rows on their dedicated deterministic lanes, routed against the
  // supplied pristine router copy. build() calls this with a snapshot taken
  // at its own entry; a windowed composition passes a snapshot taken before
  // ANY pass routed through the shared router — same state, so the rows
  // are byte-identical at any generation point (the order-decoupling law).
  // Rows are emission-ordered; sort with sortForReplay() before cursoring.
  [[nodiscard]] std::vector<transactions::Transaction>
  buildFamilyRows(const blueprints::LegitBlueprint &plan,
                  const ::PhantomLedger::infra::Router *familyRouter) const;

private:
  [[nodiscard]] const entity::account::Registry *accounts() const noexcept;

  random::Rng *rng_ = nullptr;
  blueprints::LegitTimeframe timeframe_{};
  blueprints::AccountCensus census_{};
  blueprints::CounterpartyPools counterparties_{};
  blueprints::PersonaCatalog personas_{};
  blueprints::HubSelectionRules hubSelection_{};

  OpeningBook openingBook_{};

  passes::IncomePass income_{};
  passes::RoutinePass routines_{};
  passes::FamilyPass family_{};
  passes::CreditLifecyclePass credit_{};

  FamilyTransferScenario familyScenario_{};
  const ::PhantomLedger::infra::Router *router_ = nullptr;
};

} // namespace PhantomLedger::transfers::legit::ledger
