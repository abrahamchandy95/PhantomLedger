#include "phantomledger/transfers/legit/ledger/passes.hpp"

#include "phantomledger/inflows/rent.hpp"
#include "phantomledger/inflows/revenue/generate.hpp"
#include "phantomledger/inflows/salary.hpp"
#include "phantomledger/inflows/types.hpp"
#include "phantomledger/math/amounts.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transfers/government/disability.hpp"
#include "phantomledger/transfers/government/recipients.hpp"
#include "phantomledger/transfers/government/retirement.hpp"
#include "phantomledger/transfers/legit/ledger/seeded_screen.hpp"
#include "phantomledger/transfers/legit/routines/atm.hpp"
#include "phantomledger/transfers/legit/routines/credit_cards.hpp"
#include "phantomledger/transfers/legit/routines/internal.hpp"
#include "phantomledger/transfers/legit/routines/paychecks.hpp"
#include "phantomledger/transfers/legit/routines/relatives.hpp"
#include "phantomledger/transfers/legit/routines/spending.hpp"
#include "phantomledger/transfers/legit/routines/subscriptions.hpp"

#include <cstdint>
#include <functional>
#include <span>
#include <stdexcept>

namespace PhantomLedger::transfers::legit::ledger::passes {

namespace {

namespace pl_gov = ::PhantomLedger::transfers::government;
namespace pl_inflows = ::PhantomLedger::inflows;

[[nodiscard]] time::Window
windowFromPlan(const blueprints::LegitBlueprint &plan) noexcept {
  return time::Window{
      .start = plan.startDate(),
      .days = plan.days(),
  };
}

[[nodiscard]] std::uint32_t
populationCount(const blueprints::LegitBlueprint &plan) {
  if (plan.personas().pack == nullptr) {
    throw std::invalid_argument("passes requires a populated PersonaPlan.pack");
  }

  return static_cast<std::uint32_t>(
      plan.personas().pack->table.byPerson.size());
}

[[nodiscard]] pl_inflows::HubAccounts
buildHubAccounts(const blueprints::LegitBlueprint &plan) {
  pl_inflows::HubAccounts hubs;
  hubs.reserve(plan.counterparties().hubSet.size());
  hubs.insert(plan.counterparties().hubSet.begin(),
              plan.counterparties().hubSet.end());
  return hubs;
}

[[nodiscard]] pl_inflows::PayrollCounterparties
buildPayrollCounterparties(const blueprints::LegitBlueprint &plan) {
  return pl_inflows::PayrollCounterparties{
      .employers =
          std::span<const entity::Key>(plan.counterparties().employers.data(),
                                       plan.counterparties().employers.size()),
  };
}

[[nodiscard]] pl_inflows::RentCounterparties
buildRentCounterparties(const blueprints::LegitBlueprint &plan) {
  return pl_inflows::RentCounterparties{
      .landlords =
          std::span<const entity::Key>(plan.counterparties().landlords.data(),
                                       plan.counterparties().landlords.size()),
      .landlordTypes = &plan.counterparties().landlordTypeOf,
  };
}

[[nodiscard]] pl_inflows::RevenueCounterparties
buildRevenueCounterparties(const IncomePass &pass) {
  return pl_inflows::RevenueCounterparties{
      .directory = pass.revenueCounterparties(),
  };
}

[[nodiscard]] pl_inflows::InflowSnapshot
buildInflowSnapshot(const IncomePass &pass,
                    const blueprints::LegitBlueprint &plan,
                    const entity::account::Ownership &ownership,
                    const entity::account::Registry &registry) {
  auto snapshot = pl_inflows::InflowSnapshot{
      .timeframe =
          pl_inflows::Timeframe{
              .startDate = plan.startDate(),
              .days = plan.days(),
              .monthStarts = plan.monthStarts(),
          },
      .entropy =
          pl_inflows::Entropy{
              .seed = plan.seed(),
              .factory = random::RngFactory{plan.seed()},
          },
      .population =
          pl_inflows::Population{
              populationCount(plan),
              registry,
              ownership,
              plan.personas().pack->assignment,
              buildHubAccounts(plan),
          },
      .payroll = buildPayrollCounterparties(plan),
      .rent = buildRentCounterparties(plan),
      .revenue = buildRevenueCounterparties(pass),
      .recurring = pass.recurring(),
  };

  primitives::validate::require(snapshot);
  return snapshot;
}

[[nodiscard]] pl_gov::Population
buildGovernmentPopulation(const blueprints::LegitBlueprint &plan,
                          const entity::account::Ownership &ownership,
                          const entity::account::Registry &registry) {
  return pl_gov::Population{
      .count = populationCount(plan),
      .personas = &plan.personas().pack->assignment,
      .accounts = &registry,
      .ownership = &ownership,
  };
}

} // namespace

bool GovernmentCounterparties::valid() const noexcept {
  const entity::Key sentinel{};
  return !(ssa == sentinel) && !(disability == sentinel);
}

namespace {

[[nodiscard]] random::Rng &routineRng(const RoutinePass &pass) {
  if (pass.rng() == nullptr) {
    throw std::invalid_argument("passes::addRoutines requires a non-null rng");
  }

  return *pass.rng();
}

[[nodiscard]] const transactions::Factory &routineTxf(const RoutinePass &pass) {
  if (pass.txf() == nullptr) {
    throw std::invalid_argument(
        "passes::addRoutines requires a transaction factory");
  }

  return *pass.txf();
}

[[nodiscard]] AccountAccess routineAccounts(const RoutinePass &pass) {
  const auto accounts = pass.accounts();
  if (accounts.registry == nullptr || accounts.ownership == nullptr) {
    throw std::invalid_argument(
        "passes::addRoutines requires accounts and ownership");
  }

  return accounts;
}

void addSplitDeposits(const RoutinePass &pass,
                      const blueprints::LegitBlueprint &plan,
                      TxnStreams &streams) {
  const auto accounts = routineAccounts(pass);

  streams.add(routines::paychecks::splitDeposits(
      routineRng(pass), plan, routineTxf(pass), *accounts.ownership,
      *accounts.registry,
      std::span<const transactions::Transaction>(streams.candidates())));
}

void addRent(const RoutinePass &pass, const blueprints::LegitBlueprint &plan,
             TxnStreams &streams) {
  auto &rng = routineRng(pass);
  const auto accounts = routineAccounts(pass);

  const auto incomePass = IncomePass{
      pass.rng(),
      accounts,
      nullptr,
      pass.recurring(),
  };
  const auto snap = buildInflowSnapshot(incomePass, plan, *accounts.ownership,
                                        *accounts.registry);

  const std::function<double()> rentModel = [&rng]() -> double {
    return ::PhantomLedger::math::amounts::kRent.sample(rng);
  };

  streams.add(
      pl_inflows::generateRentTxns(snap, rng, routineTxf(pass), rentModel));
}

void addSubscriptions(const RoutinePass &pass,
                      const blueprints::LegitBlueprint &plan,
                      TxnStreams &streams, ScreenBook &screen) {
  const auto accounts = routineAccounts(pass);

  auto subscriptionScreen = SeededScreen::sorted(
      screen.fresh(),
      std::span<const transactions::Transaction>(streams.screened()));
  auto subscriptions = routines::subscriptions::Generator{
      routineRng(pass),
      routineTxf(pass),
      subscriptionScreen,
  };
  streams.add(subscriptions.generate(plan, *accounts.registry));
}

void addAtm(const RoutinePass &pass, const blueprints::LegitBlueprint &plan,
            TxnStreams &streams, ScreenBook &screen) {
  const auto accounts = routineAccounts(pass);

  auto atmScreen = SeededScreen::sorted(
      screen.fresh(),
      std::span<const transactions::Transaction>(streams.screened()));
  auto atmWithdrawals =
      routines::atm::Generator{routineRng(pass), routineTxf(pass), atmScreen};
  streams.add(atmWithdrawals.generate(plan, *accounts.registry));
}

void addInternalTransfers(const RoutinePass &pass,
                          const blueprints::LegitBlueprint &plan,
                          TxnStreams &streams, ScreenBook &screen) {
  auto internalScreen = SeededScreen::sorted(
      screen.fresh(),
      std::span<const transactions::Transaction>(streams.screened()));
  auto internalTransfers = routines::internal_xfer::Generator{
      routineRng(pass),
      routineTxf(pass),
      internalScreen,
  };
  streams.add(internalTransfers.generate(plan));
}

void addSpending(const RoutinePass &pass,
                 const blueprints::LegitBlueprint &plan, TxnStreams &streams,
                 ScreenBook &screen) {
  const auto accounts = routineAccounts(pass);
  const auto resources = pass.resources();

  if (resources.accountsLookup == nullptr) {
    throw std::invalid_argument(
        "spending routine requires a non-null accountsLookup");
  }

  streams.add(routines::spending::generateDayToDayTxns(
      routines::spending::SpendingRoutine::Execution{
          .rng = routineRng(pass),
          .txf = routineTxf(pass),
      },
      routines::spending::SpendingRoutine::CensusSource{
          .blueprint = plan,
          .accounts =
              routines::spending::SpendingRoutine::AccountSource{
                  .lookup = *resources.accountsLookup,
                  .registry = *accounts.registry,
              },
      },
      routines::spending::SpendingRoutine::PayeeDirectory{
          .merchants = resources.merchants,
          .creditCards = resources.creditCards,
      },
      routines::spending::SpendingRoutine::ObligationSource{
          .portfolios = resources.portfolios,
      },
      routines::spending::SpendingRoutine::LedgerReplay{
          .baseTxns =
              std::span<const transactions::Transaction>(streams.screened()),
          .screenBook = screen.fresh(),
          .baseTxnsSorted = true,
      }));
}

} // namespace

void addIncome(const IncomePass &pass, const blueprints::LegitBlueprint &plan,
               const transactions::Factory &txf, TxnStreams &streams,
               const GovernmentCounterparties &govCps) {
  if (pass.rng() == nullptr) {
    throw std::invalid_argument("passes::addIncome requires a non-null rng");
  }

  const auto accounts = pass.accounts();
  if (accounts.registry == nullptr || accounts.ownership == nullptr) {
    throw std::invalid_argument(
        "passes::addIncome requires accounts and ownership");
  }

  auto &mainRng = *pass.rng();

  const auto snap =
      buildInflowSnapshot(pass, plan, *accounts.ownership, *accounts.registry);

  const std::function<double()> salaryModel = [&mainRng]() -> double {
    return ::PhantomLedger::math::amounts::kSalary.sample(mainRng);
  };

  streams.add(pl_inflows::generateSalaryTxns(snap, mainRng, txf, salaryModel));

  const auto benefits = pass.benefits();
  if (govCps.valid() && benefits.retirement != nullptr &&
      benefits.disability != nullptr) {
    const auto govPopulation = buildGovernmentPopulation(
        plan, *accounts.ownership, *accounts.registry);
    const auto window = windowFromPlan(plan);

    streams.add(pl_gov::retirementBenefits(
        *benefits.retirement, window, mainRng, txf, govPopulation, govCps.ssa));

    streams.add(pl_gov::disabilityBenefits(*benefits.disability, window,
                                           mainRng, txf, govPopulation,
                                           govCps.disability));
  }

  streams.add(pl_inflows::revenue::generate(snap, txf));
}

void addRoutines(const RoutinePass &pass,
                 const blueprints::LegitBlueprint &plan, TxnStreams &streams,
                 ScreenBook &screen) {
  (void)routineRng(pass);
  (void)routineTxf(pass);
  (void)routineAccounts(pass);

  addSplitDeposits(pass, plan, streams);
  addRent(pass, plan, streams);
  addSubscriptions(pass, plan, streams, screen);
  addAtm(pass, plan, streams, screen);
  addInternalTransfers(pass, plan, streams, screen);
  addSpending(pass, plan, streams, screen);
}

void addFamily(
    const ::PhantomLedger::transfers::legit::routines::family::TransferRun &run,
    const routines::relatives::FamilyTransferModel &transferModel,
    TxnStreams &streams) {
  streams.add(routines::relatives::generateFamilyTxns(run, transferModel));
}

void addCredit(const CreditLifecyclePass &pass,
               const blueprints::LegitBlueprint &plan,
               const transactions::Factory &txf, TxnStreams &streams) {
  streams.add(routines::credit_cards::generateLifecycle(
      routines::credit_cards::LifecycleRunRequest{
          .rng = pass.rng(),
          .cards = pass.cards(),
          .lifecycle = pass.lifecycle(),
      },
      plan, txf,
      std::span<const transactions::Transaction>(streams.candidates())));
}

} // namespace PhantomLedger::transfers::legit::ledger::passes
