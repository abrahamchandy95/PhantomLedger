#include "phantomledger/pipeline/stages/transfers/legit_assembly.hpp"

#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transfers/legit/ledger/limits.hpp"
#include "phantomledger/transfers/legit/ledger/passes.hpp"

namespace PhantomLedger::pipeline::stages::transfers {

namespace {

namespace blueprints = ::PhantomLedger::transfers::legit::blueprints;
namespace legit_ledger = ::PhantomLedger::transfers::legit::ledger;
namespace validate = ::PhantomLedger::primitives::validate;

void validateHubFraction(double value) {
  validate::Report report;
  report.check([&] { validate::unit("hubFraction", value); });
  report.throwIfFailed();
}

[[nodiscard]] blueprints::LegitTimeframe
makeLegitTimeframe(::PhantomLedger::time::Window window,
                   std::uint64_t seed) noexcept {
  return blueprints::LegitTimeframe{
      .window = window,
      .seed = seed,
  };
}

[[nodiscard]] blueprints::AccountCensus makeAccountCensus(
    const ::PhantomLedger::pipeline::Entities &entities) noexcept {
  return blueprints::AccountCensus{
      .accounts = &entities.accounts.registry,
      .ownership = &entities.accounts.ownership,
  };
}

[[nodiscard]] blueprints::CounterpartyPools makeCounterpartyPools(
    const ::PhantomLedger::pipeline::Entities &entities) noexcept {
  return blueprints::CounterpartyPools{
      .directory = &entities.counterparties,
      .landlords = &entities.landlords.roster,
  };
}

[[nodiscard]] blueprints::PersonaCatalog makePersonaCatalog(
    const ::PhantomLedger::pipeline::Entities &entities) noexcept {
  return blueprints::PersonaCatalog{
      .pack = &entities.personas,
  };
}

[[nodiscard]] blueprints::HubSelectionRules
makeHubSelection(const ::PhantomLedger::pipeline::Entities &entities,
                 double hubFraction) noexcept {
  return blueprints::HubSelectionRules{
      .populationCount = entities.people.roster.count,
      .fraction = hubFraction,
  };
}

[[nodiscard]] legit_ledger::OpeningBook makeOpeningBook(
    ::PhantomLedger::random::Rng &rng,
    const ::PhantomLedger::pipeline::Entities &entities,
    const ::PhantomLedger::clearing::BalanceRules *balanceRules) noexcept {
  return legit_ledger::OpeningBook{
      rng,
      legit_ledger::OpeningBook::Accounts{
          .registry = &entities.accounts.registry,
          .lookup = &entities.accounts.lookup,
          .ownership = &entities.accounts.ownership,
      },
      legit_ledger::OpeningBook::Protections{
          .balanceRules = balanceRules,
          .portfolios = &entities.portfolios,
          .creditCards = &entities.creditCards,
      },
  };
}

[[nodiscard]] legit_ledger::passes::IncomePass makeIncomePass(
    ::PhantomLedger::random::Rng &rng,
    const ::PhantomLedger::pipeline::Entities &entities,
    const ::PhantomLedger::inflows::RecurringIncomeRules &recurringIncome,
    const ::PhantomLedger::transfers::government::RetirementTerms &retirement,
    const ::PhantomLedger::transfers::government::DisabilityTerms &disability) {
  return legit_ledger::passes::IncomePass{
      &rng,
      legit_ledger::passes::AccountAccess{
          .registry = &entities.accounts.registry,
          .ownership = &entities.accounts.ownership,
      },
      &entities.counterparties,
      recurringIncome,
      legit_ledger::passes::GovernmentBenefits{
          .retirement = &retirement,
          .disability = &disability,
      },
  };
}

[[nodiscard]] legit_ledger::passes::RoutinePass makeRoutinePass(
    ::PhantomLedger::random::Rng &rng,
    const ::PhantomLedger::pipeline::Entities &entities,
    const ::PhantomLedger::inflows::RecurringIncomeRules &recurringIncome) {
  return legit_ledger::passes::RoutinePass{
      &rng,
      legit_ledger::passes::AccountAccess{
          .registry = &entities.accounts.registry,
          .ownership = &entities.accounts.ownership,
      },
      legit_ledger::passes::RoutineResources{
          .accountsLookup = &entities.accounts.lookup,
          .merchants = &entities.merchants.catalog,
          .portfolios = &entities.portfolios,
          .creditCards = &entities.creditCards,
      },
      recurringIncome,
  };
}

[[nodiscard]] legit_ledger::passes::FamilyPass
makeFamilyPass(const ::PhantomLedger::pipeline::Entities &entities) {
  return legit_ledger::passes::FamilyPass{
      legit_ledger::passes::AccountAccess{
          .registry = &entities.accounts.registry,
          .ownership = &entities.accounts.ownership,
      },
      &entities.merchants.catalog,
  };
}

[[nodiscard]] legit_ledger::passes::CreditLifecyclePass
makeCreditPass(::PhantomLedger::random::Rng &rng,
               const ::PhantomLedger::pipeline::Entities &entities,
               const ::PhantomLedger::transfers::credit_cards::LifecycleRules
                   *lifecycleRules) {
  return legit_ledger::passes::CreditLifecyclePass{
      &rng,
      &entities.creditCards,
      lifecycleRules,
  };
}

} // namespace

LegitAssembly &LegitAssembly::runScope(RunScope value) noexcept {
  run_ = value;
  return *this;
}

LegitAssembly &LegitAssembly::incomePrograms(const IncomePrograms &value) {
  income_ = value;
  return *this;
}

LegitAssembly &LegitAssembly::openingBalances(OpeningBalances value) noexcept {
  openingBalances_ = value;
  return *this;
}

LegitAssembly &LegitAssembly::cardLifecycle(CardLifecycle value) noexcept {
  cardLifecycle_ = value;
  return *this;
}

LegitAssembly &
LegitAssembly::familyTransfers(FamilyTransferScenario value) noexcept {
  familyTransfers_ = value;
  return *this;
}

LegitAssembly &LegitAssembly::hubSelection(HubSelection value) noexcept {
  hubSelection_ = value;
  return *this;
}

LegitAssembly &
LegitAssembly::window(::PhantomLedger::time::Window value) noexcept {
  run_.window = value;
  return *this;
}

LegitAssembly &LegitAssembly::seed(std::uint64_t value) noexcept {
  run_.seed = value;
  return *this;
}

LegitAssembly &LegitAssembly::recurringIncome(
    const ::PhantomLedger::inflows::RecurringIncomeRules &value) {
  income_.recurring = value;
  return *this;
}

LegitAssembly &LegitAssembly::employmentRules(
    const ::PhantomLedger::recurring::EmploymentRules &value) {
  income_.recurring.employment = value;
  return *this;
}

LegitAssembly &
LegitAssembly::leaseRules(const ::PhantomLedger::recurring::LeaseRules &value) {
  income_.recurring.lease = value;
  return *this;
}

LegitAssembly &LegitAssembly::salaryPaidFraction(double value) noexcept {
  income_.recurring.salaryPaidFraction = value;
  return *this;
}

LegitAssembly &LegitAssembly::rentPaidFraction(double value) noexcept {
  income_.recurring.rentPaidFraction = value;
  return *this;
}

LegitAssembly &LegitAssembly::openingBalanceRules(
    const ::PhantomLedger::clearing::BalanceRules *value) noexcept {
  openingBalances_.balanceRules = value;
  return *this;
}

LegitAssembly &LegitAssembly::creditLifecycle(
    const ::PhantomLedger::transfers::credit_cards::LifecycleRules
        *value) noexcept {
  cardLifecycle_.lifecycleRules = value;
  return *this;
}

LegitAssembly &LegitAssembly::family(FamilyTransferScenario value) noexcept {
  familyTransfers_ = value;
  return *this;
}

LegitAssembly &LegitAssembly::retirementBenefits(
    const ::PhantomLedger::transfers::government::RetirementTerms &value) {
  income_.retirement = value;
  return *this;
}

LegitAssembly &LegitAssembly::disabilityBenefits(
    const ::PhantomLedger::transfers::government::DisabilityTerms &value) {
  income_.disability = value;
  return *this;
}

void LegitAssembly::validate() const {
  validateHubFraction(hubSelection_.fraction);
}

legit_ledger::LegitTransferBuilder
LegitAssembly::builder(::PhantomLedger::random::Rng &rng,
                       const ::PhantomLedger::pipeline::Entities &entities,
                       const ::PhantomLedger::pipeline::Infra &infra) const {
  legit_ledger::LegitTransferBuilder out{
      rng,
      makeLegitTimeframe(run_.window, run_.seed),
      makeAccountCensus(entities),
      makeOpeningBook(rng, entities, openingBalances_.balanceRules),
  };

  out.counterparties(makeCounterpartyPools(entities))
      .personas(makePersonaCatalog(entities))
      .hubSelection(makeHubSelection(entities, hubSelection_.fraction))
      .income(makeIncomePass(rng, entities, income_.recurring,
                             income_.retirement, income_.disability))
      .routines(makeRoutinePass(rng, entities, income_.recurring))
      .family(makeFamilyPass(entities))
      .credit(makeCreditPass(rng, entities, cardLifecycle_.lifecycleRules))
      .familyScenario(familyTransfers_)
      .router(infra.router);

  return out;
}

} // namespace PhantomLedger::pipeline::stages::transfers
