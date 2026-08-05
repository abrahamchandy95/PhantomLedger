#include "phantomledger/transfers/legit/assembly.hpp"

#include "phantomledger/entities/counterparties/institutional_accounts.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/relationships/family/links.hpp"
#include "phantomledger/relationships/family/partition.hpp"
#include "phantomledger/relationships/family/support.hpp"
#include "phantomledger/transfers/legit/ledger/limits.hpp"
#include "phantomledger/transfers/legit/ledger/passes.hpp"
#include "phantomledger/transfers/legit/routines/relatives.hpp"

namespace PhantomLedger::transfers::legit {

namespace {

namespace legit_ledger = ::PhantomLedger::transfers::legit::ledger;
namespace validate = ::PhantomLedger::primitives::validate;
namespace income = ::PhantomLedger::activity::income;
namespace counterparties = ::PhantomLedger::counterparties;

[[nodiscard]] FamilyTransferScenario makeDefaultFamilyScenario() {
  namespace family_rel = ::PhantomLedger::relationships::family;
  namespace relatives = ::PhantomLedger::transfers::legit::routines::relatives;

  FamilyTransferScenario out;
  out.households(family_rel::kDefaultHouseholds)
      .dependents(family_rel::kDefaultDependents)
      .retireeSupport(family_rel::kDefaultRetireeSupport)
      .transfers(relatives::kDefaultFamilyTransferModel);
  return out;
}

void validateHubFraction(double value) {
  validate::Report report;
  report.check([&] { validate::unit("hubFraction", value); });
  report.throwIfFailed();
}

[[nodiscard]] legit_ledger::OpeningBook makeOpeningBook(
    ::PhantomLedger::random::Rng &rng,
    const LegitAssembly::WorldInputs &world,
    const ::PhantomLedger::clearing::BalanceRules *balanceRules) noexcept {
  return legit_ledger::OpeningBook{
      rng,
      legit_ledger::OpeningBook::Accounts{
          .registry = &world.accounts->registry,
          .lookup = &world.accounts->lookup,
          .ownership = &world.accounts->ownership,
      },
      legit_ledger::OpeningBook::Protections{
          .balanceRules = balanceRules,
          .portfolios = world.portfolios,
          .creditCards = world.creditCards,
      },
  };
}

} // namespace

LegitAssembly::LegitAssembly()
    : familyTransfers_(makeDefaultFamilyScenario()) {}

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

LegitAssembly &LegitAssembly::salaryRules(const income::salary::Rules &value) {
  income_.salary = value;
  return *this;
}

LegitAssembly &LegitAssembly::rentRules(const income::rent::Rules &value) {
  income_.rent = value;
  return *this;
}

LegitAssembly &LegitAssembly::employmentRules(
    const ::PhantomLedger::activity::recurring::EmploymentRules &value) {
  income_.salary.employment = value;
  return *this;
}

LegitAssembly &LegitAssembly::leaseRules(
    const ::PhantomLedger::activity::recurring::LeaseRules &value) {
  income_.rent.lease = value;
  return *this;
}

LegitAssembly &LegitAssembly::salaryPaidFraction(double value) noexcept {
  income_.salary.paidFraction = value;
  return *this;
}

LegitAssembly &LegitAssembly::rentPaidFraction(double value) noexcept {
  income_.rent.paidFraction = value;
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

legit_ledger::LegitTransferBuilder LegitAssembly::builder(
    ::PhantomLedger::random::Rng &rng, const WorldInputs &world) const {
  legit_ledger::LegitTransferBuilder out{
      rng,
      blueprints::LegitTimeframe{
          .window = run_.window,
          .seed = run_.seed,
      },
      blueprints::AccountCensus{
          .accounts = &world.accounts->registry,
          .ownership = &world.accounts->ownership,
      },
      makeOpeningBook(rng, world, openingBalances_.balanceRules),
  };

  const legit_ledger::passes::AccountAccess accountAccess{
      .registry = &world.accounts->registry,
      .ownership = &world.accounts->ownership,
  };

  out.counterparties(blueprints::CounterpartyPools{
                         .directory = world.counterparties,
                         .landlords = &world.landlords->roster,
                     })
      .personas(blueprints::PersonaCatalog{
          .pack = world.personas,
      })
      .hubSelection(blueprints::HubSelectionRules{
          .populationCount = world.populationCount,
          .fraction = hubSelection_.fraction,
      })
      .income(legit_ledger::passes::IncomePass{
          &rng,
          accountAccess,
          legit_ledger::passes::SalarySetup{
              .revenueCounterparties = world.counterparties,
              .rules = income_.salary,
          },
          legit_ledger::passes::GovernmentSetup{
              .counterparties =
                  legit_ledger::passes::GovernmentCounterparties{
                      .ssa =
                          counterparties::key(counterparties::Government::ssa),
                      .disability = counterparties::key(
                          counterparties::Government::disability),
                  },
              .retirement = &income_.retirement,
              .disability = &income_.disability,
          },
      })
      .routines(legit_ledger::passes::RoutinePass{
          &rng,
          accountAccess,
          legit_ledger::passes::RoutineResources{
              .accountsLookup = &world.accounts->lookup,
              .merchants = world.merchants,
              .portfolios = world.portfolios,
              .creditCards = world.creditCards,
              .cardLifecycle = cardLifecycle_.lifecycleRules,
              // geo-causal-v1 (G2a step-2): carry the per-person home areas
              // into the monolith spending pass so the reference oracle
              // selects from the SAME homes as the windowed path.
              .homeAreas = world.homeAreas,
              // relocation-2026-07: the home-area HISTORY, so the monolith
              // oracle refreshes homes from the same schedule the windowed
              // engine does. An asymmetry here IS an engine divergence.
              .relocation = world.relocation,
          },
          income_.rent,
      })
      .family(legit_ledger::passes::FamilyPass{
          accountAccess,
          world.merchants,
      })
      .credit(legit_ledger::passes::CreditLifecyclePass{
          &rng,
          world.creditCards,
          cardLifecycle_.lifecycleRules,
      })
      .familyScenario(familyTransfers_);

  return out;
}

} // namespace PhantomLedger::transfers::legit
