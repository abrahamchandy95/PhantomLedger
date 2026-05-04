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
windowFromPlan(const blueprints::LegitBuildPlan &plan) noexcept {
  return time::Window{
      .start = plan.startDate,
      .days = plan.days,
  };
}

[[nodiscard]] std::uint32_t
populationCount(const blueprints::LegitBuildPlan &plan) {
  if (plan.personas.pack == nullptr) {
    throw std::invalid_argument("passes requires a populated PersonaPlan.pack");
  }

  return static_cast<std::uint32_t>(plan.personas.pack->table.byPerson.size());
}

[[nodiscard]] pl_inflows::HubAccounts
buildHubAccounts(const blueprints::LegitBuildPlan &plan) {
  pl_inflows::HubAccounts hubs;
  hubs.reserve(plan.counterparties.hubSet.size());
  hubs.insert(plan.counterparties.hubSet.begin(),
              plan.counterparties.hubSet.end());
  return hubs;
}

[[nodiscard]] pl_inflows::PayrollCounterparties
buildPayrollCounterparties(const blueprints::LegitBuildPlan &plan) {
  return pl_inflows::PayrollCounterparties{
      .employers =
          std::span<const entity::Key>(plan.counterparties.employers.data(),
                                       plan.counterparties.employers.size()),
  };
}

[[nodiscard]] pl_inflows::RentCounterparties
buildRentCounterparties(const blueprints::LegitBuildPlan &plan) {
  return pl_inflows::RentCounterparties{
      .landlords =
          std::span<const entity::Key>(plan.counterparties.landlords.data(),
                                       plan.counterparties.landlords.size()),
      .landlordTypes = &plan.counterparties.landlordTypeOf,
  };
}

[[nodiscard]] pl_inflows::RevenueCounterparties
buildRevenueCounterparties(const IncomePassRequest &request) {
  return pl_inflows::RevenueCounterparties{
      .directory = request.revenueCounterparties,
  };
}

[[nodiscard]] pl_inflows::InflowSnapshot
buildInflowSnapshot(const IncomePassRequest &request,
                    const blueprints::LegitBuildPlan &plan,
                    const entity::account::Ownership &ownership,
                    const entity::account::Registry &registry) {
  auto snapshot = pl_inflows::InflowSnapshot{
      .timeframe =
          pl_inflows::Timeframe{
              .startDate = plan.startDate,
              .days = plan.days,
              .monthStarts = plan.monthStarts,
          },
      .entropy =
          pl_inflows::Entropy{
              .seed = plan.seed,
              .factory = random::RngFactory{plan.seed},
          },
      .population =
          pl_inflows::Population{
              populationCount(plan),
              registry,
              ownership,
              plan.personas.pack->assignment,
              buildHubAccounts(plan),
          },
      .payroll = buildPayrollCounterparties(plan),
      .rent = buildRentCounterparties(plan),
      .revenue = buildRevenueCounterparties(request),
      .recurring = request.recurring,
  };

  primitives::validate::require(snapshot);
  return snapshot;
}

[[nodiscard]] pl_gov::Population
buildGovernmentPopulation(const blueprints::LegitBuildPlan &plan,
                          const entity::account::Ownership &ownership,
                          const entity::account::Registry &registry) {
  return pl_gov::Population{
      .count = populationCount(plan),
      .personas = &plan.personas.pack->assignment,
      .accounts = &registry,
      .ownership = &ownership,
  };
}

} // namespace

bool GovernmentCounterparties::valid() const noexcept {
  const entity::Key sentinel{};
  return !(ssa == sentinel) && !(disability == sentinel);
}

// ---------------------------------------------------------------------------
// addIncome
// ---------------------------------------------------------------------------

void addIncome(const IncomePassRequest &request,
               const blueprints::LegitBuildPlan &plan,
               const transactions::Factory &txf, TxnStreams &streams,
               const GovernmentCounterparties &govCps) {
  if (request.rng == nullptr) {
    throw std::invalid_argument("passes::addIncome requires a non-null rng");
  }

  if (request.accounts == nullptr || request.ownership == nullptr) {
    throw std::invalid_argument(
        "passes::addIncome requires accounts and ownership");
  }

  auto &mainRng = *request.rng;

  const auto snap =
      buildInflowSnapshot(request, plan, *request.ownership, *request.accounts);

  const std::function<double()> salaryModel = [&mainRng]() -> double {
    return ::PhantomLedger::math::amounts::kSalary.sample(mainRng);
  };

  streams.add(pl_inflows::generateSalaryTxns(snap, mainRng, txf, salaryModel));

  if (govCps.valid() && request.retirement != nullptr &&
      request.disability != nullptr) {
    const auto govPopulation =
        buildGovernmentPopulation(plan, *request.ownership, *request.accounts);
    const auto window = windowFromPlan(plan);

    streams.add(pl_gov::retirementBenefits(*request.retirement, window, mainRng,
                                           txf, govPopulation, govCps.ssa));

    streams.add(pl_gov::disabilityBenefits(*request.disability, window, mainRng,
                                           txf, govPopulation,
                                           govCps.disability));
  }

  streams.add(pl_inflows::revenue::generate(snap, txf));
}

// ---------------------------------------------------------------------------
// addRoutines
// ---------------------------------------------------------------------------

void addRoutines(const RoutinePassRequest &request,
                 const blueprints::LegitBuildPlan &plan,
                 const entity::account::Ownership &ownership,
                 const entity::account::Registry &registry,
                 const transactions::Factory &txf, TxnStreams &streams,
                 ScreenBook &screen) {
  if (request.rng == nullptr) {
    throw std::invalid_argument("passes::addRoutines requires a non-null rng");
  }

  auto &rng = *request.rng;

  streams.add(routines::paychecks::splitDeposits(
      rng, plan, txf, ownership, registry,
      std::span<const transactions::Transaction>(streams.candidates())));

  const auto incomeReq = IncomePassRequest{
      .rng = request.rng,
      .accounts = &registry,
      .ownership = &ownership,
      .recurring = request.recurring,
  };
  const auto snap = buildInflowSnapshot(incomeReq, plan, ownership, registry);

  const std::function<double()> rentModel = [&rng]() -> double {
    return ::PhantomLedger::math::amounts::kRent.sample(rng);
  };

  streams.add(pl_inflows::generateRentTxns(snap, rng, txf, rentModel));

  streams.add(routines::subscriptions::generate(
      rng, plan, txf, registry,
      /*book=*/screen.fresh(),
      /*baseTxns=*/
      std::span<const transactions::Transaction>(streams.screened()),
      /*baseTxnsSorted=*/true));

  auto atmScreen = SeededScreen::sorted(
      screen.fresh(),
      std::span<const transactions::Transaction>(streams.screened()));
  auto atmWithdrawals = routines::atm::Generator{rng, txf, atmScreen};
  streams.add(atmWithdrawals.generate(plan, registry));

  auto internalScreen = SeededScreen::sorted(
      screen.fresh(),
      std::span<const transactions::Transaction>(streams.screened()));
  auto internalTransfers = routines::internal_xfer::Generator{
      rng,
      txf,
      internalScreen,
  };
  streams.add(internalTransfers.generate(plan));

  streams.add(routines::spending::generateDayToDayTxns(
      routines::spending::SpendingRunRequest{
          .rng = request.rng,
          .txf = &txf,
          .accountsLookup = request.accountsLookup,
          .merchants = request.merchants,
          .portfolios = request.portfolios,
          .creditCards = request.creditCards,
          .plan = plan,
          .registry = registry,
      },
      routines::spending::SpendingLedgerSeed{
          .baseTxns =
              std::span<const transactions::Transaction>(streams.screened()),
          .screenBook = screen.fresh(),
          .baseTxnsSorted = true,
      }));
}

// ---------------------------------------------------------------------------
// addFamily
// ---------------------------------------------------------------------------

void addFamily(
    const ::PhantomLedger::transfers::legit::routines::family::Runtime &runtime,
    const routines::relatives::FamilyTransferModel &transferModel,
    TxnStreams &streams) {
  streams.add(routines::relatives::generateFamilyTxns(runtime, transferModel));
}

// ---------------------------------------------------------------------------
// addCredit
// ---------------------------------------------------------------------------

void addCredit(const CreditPassRequest &request,
               const blueprints::LegitBuildPlan &plan,
               const transactions::Factory &txf, TxnStreams &streams) {
  streams.add(routines::credit_cards::generateLifecycle(
      routines::credit_cards::LifecycleRunRequest{
          .rng = request.rng,
          .cards = request.cards,
          .lifecycle = request.lifecycle,
      },
      plan, txf,
      std::span<const transactions::Transaction>(streams.candidates())));
}

} // namespace PhantomLedger::transfers::legit::ledger::passes
