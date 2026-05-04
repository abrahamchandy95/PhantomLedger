#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/cards.hpp"
#include "phantomledger/entities/counterparties.hpp"
#include "phantomledger/inflows/types.hpp"
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

struct IncomePassRequest {
  random::Rng *rng = nullptr;

  const entity::account::Registry *accounts = nullptr;
  const entity::account::Ownership *ownership = nullptr;
  const entity::counterparty::Directory *revenueCounterparties = nullptr;

  inflows::RecurringIncomeRules recurring{};

  const government::RetirementTerms *retirement = nullptr;
  const government::DisabilityTerms *disability = nullptr;
};

struct RoutinePassRequest {
  random::Rng *rng = nullptr;
  const entity::account::Lookup *accountsLookup = nullptr;
  const entity::merchant::Catalog *merchants = nullptr;
  const entity::product::PortfolioRegistry *portfolios = nullptr;
  const entity::card::Registry *creditCards = nullptr;

  inflows::RecurringIncomeRules recurring{};
};

struct FamilyPassRequest {
  const entity::account::Registry *accounts = nullptr;
  const entity::account::Ownership *ownership = nullptr;
  const entity::merchant::Catalog *merchants = nullptr;
};

struct CreditPassRequest {
  random::Rng *rng = nullptr;
  const entity::card::Registry *cards = nullptr;
  const ::PhantomLedger::transfers::credit_cards::LifecycleRules *lifecycle =
      nullptr;
};

void addIncome(const IncomePassRequest &request,
               const blueprints::LegitBuildPlan &plan,
               const transactions::Factory &txf, TxnStreams &streams,
               const GovernmentCounterparties &govCps = {});

void addRoutines(const RoutinePassRequest &request,
                 const blueprints::LegitBuildPlan &plan,
                 const entity::account::Ownership &ownership,
                 const entity::account::Registry &registry,
                 const transactions::Factory &txf, TxnStreams &streams,
                 ScreenBook &screen);

void addFamily(
    const ::PhantomLedger::transfers::legit::routines::family::Runtime &runtime,
    const routines::relatives::FamilyTransferModel &transferModel,
    TxnStreams &streams);

void addCredit(const CreditPassRequest &request,
               const blueprints::LegitBuildPlan &plan,
               const transactions::Factory &txf, TxnStreams &streams);

} // namespace PhantomLedger::transfers::legit::ledger::passes
