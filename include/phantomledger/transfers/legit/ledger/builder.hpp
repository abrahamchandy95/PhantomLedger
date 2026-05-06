#pragma once

#include "phantomledger/transfers/legit/blueprints/plans.hpp"
#include "phantomledger/transfers/legit/ledger/limits.hpp"
#include "phantomledger/transfers/legit/ledger/passes.hpp"
#include "phantomledger/transfers/legit/ledger/result.hpp"
#include "phantomledger/transfers/legit/routines/relatives.hpp"

namespace PhantomLedger::infra {
class Router;
} // namespace PhantomLedger::infra

namespace PhantomLedger::transfers::legit::ledger {

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
