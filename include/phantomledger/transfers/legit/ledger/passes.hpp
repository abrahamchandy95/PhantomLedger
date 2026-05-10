#pragma once

#include "phantomledger/activity/income/rent.hpp"
#include "phantomledger/activity/income/salary.hpp"
#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/cards.hpp"
#include "phantomledger/entities/counterparties.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"
#include "phantomledger/transfers/legit/ledger/screenbook.hpp"
#include "phantomledger/transfers/legit/ledger/streams.hpp"
#include "phantomledger/transfers/legit/routines/relatives.hpp"

namespace PhantomLedger::entity::merchant {
struct Catalog;
} // namespace PhantomLedger::entity::merchant

namespace PhantomLedger::entity::product {
class PortfolioRegistry;
} // namespace PhantomLedger::entity::product

namespace PhantomLedger::transfers::credit_cards {
struct LifecycleRules;
} // namespace PhantomLedger::transfers::credit_cards

namespace PhantomLedger::transfers::government {
struct DisabilityTerms;
struct RetirementTerms;
} // namespace PhantomLedger::transfers::government

namespace PhantomLedger::relationships::family {
struct Households;
struct Dependents;
struct RetireeSupport;
} // namespace PhantomLedger::relationships::family

namespace PhantomLedger::transfers::legit::ledger::passes {

struct GovernmentCounterparties {
  entity::Key ssa{};
  entity::Key disability{};

  [[nodiscard]] bool valid() const noexcept;
};

struct AccountAccess {
  const entity::account::Registry *registry = nullptr;
  const entity::account::Ownership *ownership = nullptr;
};

/// Salary-side income inputs: the directory of revenue counterparties
/// (employers, payors, etc.) plus the rules for sizing salary draws.
struct SalarySetup {
  const entity::counterparty::Directory *revenueCounterparties = nullptr;
  inflows::salary::Rules rules{};
};

/// Government-benefits configuration: the source counterparties (SSA,
/// disability office) paired with the term tables that describe the
/// retirement and disability programs they administer. A
/// default-constructed instance has `counterparties.valid() == false`,
/// in which case the gov-benefits branch is skipped.
struct GovernmentSetup {
  GovernmentCounterparties counterparties{};
  const government::RetirementTerms *retirement = nullptr;
  const government::DisabilityTerms *disability = nullptr;

  [[nodiscard]] bool active() const noexcept {
    return counterparties.valid() && retirement != nullptr &&
           disability != nullptr;
  }
};

struct RoutineResources {
  const entity::account::Lookup *accountsLookup = nullptr;
  const entity::merchant::Catalog *merchants = nullptr;
  const entity::product::PortfolioRegistry *portfolios = nullptr;
  const entity::card::Registry *creditCards = nullptr;
};

class IncomePass {
public:
  IncomePass() = default;

  IncomePass(random::Rng *rng, AccountAccess accounts, SalarySetup salary,
             GovernmentSetup government = {}) noexcept
      : rng_(rng), accounts_(accounts), salary_(std::move(salary)),
        government_(government) {}

  [[nodiscard]] random::Rng *rng() const noexcept { return rng_; }

  [[nodiscard]] AccountAccess accounts() const noexcept { return accounts_; }

  [[nodiscard]] const SalarySetup &salary() const noexcept { return salary_; }

  [[nodiscard]] const GovernmentSetup &government() const noexcept {
    return government_;
  }

private:
  random::Rng *rng_ = nullptr;
  AccountAccess accounts_{};
  SalarySetup salary_{};
  GovernmentSetup government_{};
};

class RoutinePass {
public:
  RoutinePass() = default;

  RoutinePass(random::Rng *rng, AccountAccess accounts,
              RoutineResources resources,
              inflows::rent::Rules rentRules = {}) noexcept
      : rng_(rng), accounts_(accounts), resources_(resources),
        rentRules_(std::move(rentRules)) {}

  RoutinePass(random::Rng *rng, RoutineResources resources,
              inflows::rent::Rules rentRules = {}) noexcept
      : RoutinePass(rng, AccountAccess{}, resources, std::move(rentRules)) {}

  RoutinePass &accounts(AccountAccess value) noexcept {
    accounts_ = value;
    return *this;
  }

  RoutinePass &txf(const transactions::Factory &value) noexcept {
    txf_ = &value;
    return *this;
  }

  [[nodiscard]] random::Rng *rng() const noexcept { return rng_; }

  [[nodiscard]] AccountAccess accounts() const noexcept { return accounts_; }

  [[nodiscard]] const transactions::Factory *txf() const noexcept {
    return txf_;
  }

  [[nodiscard]] RoutineResources resources() const noexcept {
    return resources_;
  }

  [[nodiscard]] const inflows::rent::Rules &rentRules() const noexcept {
    return rentRules_;
  }

private:
  random::Rng *rng_ = nullptr;
  AccountAccess accounts_{};
  const transactions::Factory *txf_ = nullptr;
  RoutineResources resources_{};
  inflows::rent::Rules rentRules_{};
};

class FamilyPass {
public:
  FamilyPass() = default;

  FamilyPass(AccountAccess accounts,
             const entity::merchant::Catalog *merchants) noexcept
      : accounts_(accounts), merchants_(merchants) {}

  [[nodiscard]] AccountAccess accounts() const noexcept { return accounts_; }

  [[nodiscard]] const entity::merchant::Catalog *merchants() const noexcept {
    return merchants_;
  }

private:
  AccountAccess accounts_{};
  const entity::merchant::Catalog *merchants_ = nullptr;
};

class CreditLifecyclePass {
public:
  CreditLifecyclePass() = default;

  CreditLifecyclePass(
      random::Rng *rng, const entity::card::Registry *cards,
      const ::PhantomLedger::transfers::credit_cards::LifecycleRules
          *lifecycle) noexcept
      : rng_(rng), cards_(cards), lifecycle_(lifecycle) {}

  [[nodiscard]] random::Rng *rng() const noexcept { return rng_; }

  [[nodiscard]] const entity::card::Registry *cards() const noexcept {
    return cards_;
  }

  [[nodiscard]] const ::PhantomLedger::transfers::credit_cards::LifecycleRules *
  lifecycle() const noexcept {
    return lifecycle_;
  }

private:
  random::Rng *rng_ = nullptr;
  const entity::card::Registry *cards_ = nullptr;
  const ::PhantomLedger::transfers::credit_cards::LifecycleRules *lifecycle_ =
      nullptr;
};

void addIncome(const IncomePass &pass, const blueprints::LegitBlueprint &plan,
               const transactions::Factory &txf, TxnStreams &streams);

void addRoutines(const RoutinePass &pass,
                 const blueprints::LegitBlueprint &plan, TxnStreams &streams,
                 ScreenBook &screen);

void addFamily(
    const ::PhantomLedger::transfers::legit::routines::family::TransferRun &run,
    const routines::relatives::FamilyTransferModel &transferModel,
    TxnStreams &streams);

void addCredit(const CreditLifecyclePass &pass,
               const blueprints::LegitBlueprint &plan,
               const transactions::Factory &txf, TxnStreams &streams);

} // namespace PhantomLedger::transfers::legit::ledger::passes
