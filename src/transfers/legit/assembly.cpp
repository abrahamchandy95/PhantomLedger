#include "phantomledger/transfers/legit/assembly.hpp"

#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/relationships/family/links.hpp"
#include "phantomledger/relationships/family/partition.hpp"
#include "phantomledger/relationships/family/support.hpp"
#include "phantomledger/taxonomies/counterparties/accounts.hpp"
#include "phantomledger/transfers/legit/ledger/limits.hpp"
#include "phantomledger/transfers/legit/ledger/passes.hpp"
#include "phantomledger/transfers/legit/routines/relatives.hpp"

namespace PhantomLedger::transfers::legit {

namespace {

namespace blueprints = ::PhantomLedger::transfers::legit::blueprints;
namespace legit_ledger = ::PhantomLedger::transfers::legit::ledger;
namespace validate = ::PhantomLedger::primitives::validate;
namespace income = ::PhantomLedger::activity::income;
namespace pipe = ::PhantomLedger::pipeline;

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

[[nodiscard]] blueprints::LegitTimeframe
makeLegitTimeframe(::PhantomLedger::time::Window window,
                   std::uint64_t seed) noexcept {
  return blueprints::LegitTimeframe{
      .window = window,
      .seed = seed,
  };
}

[[nodiscard]] blueprints::AccountCensus
makeAccountCensus(const pipe::Holdings &holdings) noexcept {
  return blueprints::AccountCensus{
      .accounts = &holdings.accounts.registry,
      .ownership = &holdings.accounts.ownership,
  };
}

[[nodiscard]] blueprints::CounterpartyPools
makeCounterpartyPools(const pipe::Counterparties &cps) noexcept {
  return blueprints::CounterpartyPools{
      .directory = &cps.counterparties,
      .landlords = &cps.landlords.roster,
  };
}

[[nodiscard]] blueprints::PersonaCatalog
makePersonaCatalog(const pipe::People &people) noexcept {
  return blueprints::PersonaCatalog{
      .pack = &people.personas,
  };
}

[[nodiscard]] blueprints::HubSelectionRules
makeHubSelection(const pipe::People &people, double hubFraction) noexcept {
  return blueprints::HubSelectionRules{
      .populationCount = people.roster.roster.count,
      .fraction = hubFraction,
  };
}

[[nodiscard]] legit_ledger::OpeningBook makeOpeningBook(
    ::PhantomLedger::random::Rng &rng, const pipe::Holdings &holdings,
    const ::PhantomLedger::clearing::BalanceRules *balanceRules) noexcept {
  return legit_ledger::OpeningBook{
      rng,
      legit_ledger::OpeningBook::Accounts{
          .registry = &holdings.accounts.registry,
          .lookup = &holdings.accounts.lookup,
          .ownership = &holdings.accounts.ownership,
      },
      legit_ledger::OpeningBook::Protections{
          .balanceRules = balanceRules,
          .portfolios = &holdings.portfolios,
          .creditCards = &holdings.creditCards,
      },
  };
}

[[nodiscard]] legit_ledger::passes::FamilyPass
makeFamilyPass(const pipe::Holdings &holdings,
               const pipe::Counterparties &cps) {
  return legit_ledger::passes::FamilyPass{
      legit_ledger::passes::AccountAccess{
          .registry = &holdings.accounts.registry,
          .ownership = &holdings.accounts.ownership,
      },
      &cps.merchants,
  };
}

[[nodiscard]] legit_ledger::passes::CreditLifecyclePass
makeCreditPass(::PhantomLedger::random::Rng &rng,
               const pipe::Holdings &holdings,
               const ::PhantomLedger::transfers::credit_cards::LifecycleRules
                   *lifecycleRules) {
  return legit_ledger::passes::CreditLifecyclePass{
      &rng,
      &holdings.creditCards,
      lifecycleRules,
  };
}

[[nodiscard]] legit_ledger::passes::GovernmentCounterparties
defaultGovernmentCounterparties() noexcept {
  namespace counterparties = ::PhantomLedger::counterparties;
  return legit_ledger::passes::GovernmentCounterparties{
      .ssa = counterparties::key(counterparties::Government::ssa),
      .disability = counterparties::key(counterparties::Government::disability),
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
    ::PhantomLedger::random::Rng &rng, const pipe::People &people,
    const pipe::Holdings &holdings, const pipe::Counterparties &cps) const {
  legit_ledger::LegitTransferBuilder out{
      rng,
      makeLegitTimeframe(run_.window, run_.seed),
      makeAccountCensus(holdings),
      makeOpeningBook(rng, holdings, openingBalances_.balanceRules),
  };

  const legit_ledger::passes::AccountAccess accountAccess{
      .registry = &holdings.accounts.registry,
      .ownership = &holdings.accounts.ownership,
  };

  out.counterparties(makeCounterpartyPools(cps))
      .personas(makePersonaCatalog(people))
      .hubSelection(makeHubSelection(people, hubSelection_.fraction))
      .income(legit_ledger::passes::IncomePass{
          &rng,
          accountAccess,
          legit_ledger::passes::SalarySetup{
              .revenueCounterparties = &cps.counterparties,
              .rules = income_.salary,
          },
          legit_ledger::passes::GovernmentSetup{
              .counterparties = defaultGovernmentCounterparties(),
              .retirement = &income_.retirement,
              .disability = &income_.disability,
          },
      })
      .routines(legit_ledger::passes::RoutinePass{
          &rng,
          accountAccess,
          legit_ledger::passes::RoutineResources{
              .accountsLookup = &holdings.accounts.lookup,
              .merchants = &cps.merchants,
              .portfolios = &holdings.portfolios,
              .creditCards = &holdings.creditCards,
              .cardLifecycle = cardLifecycle_.lifecycleRules,
          },
          income_.rent,
      })
      .family(makeFamilyPass(holdings, cps))
      .credit(makeCreditPass(rng, holdings, cardLifecycle_.lifecycleRules))
      .familyScenario(familyTransfers_);

  return out;
}

} // namespace PhantomLedger::transfers::legit
