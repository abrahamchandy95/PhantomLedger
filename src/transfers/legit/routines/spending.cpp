#include "phantomledger/transfers/legit/routines/spending.hpp"

#include "phantomledger/entities/synth/personas/pack.hpp"
#include "phantomledger/spending/market/bootstrap.hpp"
#include "phantomledger/spending/market/cards.hpp"
#include "phantomledger/spending/market/commerce/network.hpp"
#include "phantomledger/spending/obligations/burden.hpp"
#include "phantomledger/spending/obligations/snapshot.hpp"
#include "phantomledger/spending/simulator/commerce_evolver.hpp"
#include "phantomledger/spending/simulator/day_driver.hpp"
#include "phantomledger/spending/simulator/day_source.hpp"
#include "phantomledger/spending/simulator/driver.hpp"
#include "phantomledger/spending/simulator/engine.hpp"
#include "phantomledger/spending/simulator/population_dynamics.hpp"
#include "phantomledger/spending/simulator/run_planner.hpp"
#include "phantomledger/spending/simulator/spender_emission_driver.hpp"
#include "phantomledger/transfers/legit/blueprints/paydays.hpp"
#include "phantomledger/transfers/legit/ledger/burdens.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace PhantomLedger::transfers::legit::routines::spending {

namespace plSpending = ::PhantomLedger::spending;
namespace plMarket = plSpending::market;
namespace plPop = plMarket::population;
namespace plObligations = plSpending::obligations;
namespace plSimulator = plSpending::simulator;

SpendingRoutine &SpendingRoutine::habits(SpendingHabits value) noexcept {
  habits_ = value;
  return *this;
}

SpendingRoutine &SpendingRoutine::planning(RunPlanning value) noexcept {
  planning_ = value;
  return *this;
}

SpendingRoutine &SpendingRoutine::dayPattern(DayPattern value) noexcept {
  day_ = value;
  return *this;
}

SpendingRoutine &SpendingRoutine::dynamics(DynamicsProfile value) noexcept {
  dynamics_ = value;
  return *this;
}

SpendingRoutine &SpendingRoutine::emission(EmissionProfile value) noexcept {
  emission_ = value;
  return *this;
}

namespace {

// ---------------------------------------------------------------------------
// Census materialization
// ---------------------------------------------------------------------------

struct CensusScratch {
  std::uint32_t personCount = 0;
  std::vector<entity::Key> primaryAccounts;
  std::vector<plPop::PaydaySet> paydaySets;

  std::vector<std::vector<std::uint32_t>> paydayStorage;
};

[[nodiscard]] CensusScratch
buildCensusScratch(const blueprints::LegitBuildPlan &plan,
                   const entity::account::Lookup &lookup,
                   const entity::account::Registry &registry,
                   std::span<const transactions::Transaction> baseTxns) {
  if (plan.personas.pack == nullptr) {
    throw std::invalid_argument(
        "spending routine requires a populated PersonaPlan.pack");
  }

  CensusScratch out;

  out.personCount =
      static_cast<std::uint32_t>(plan.personas.pack->table.byPerson.size());

  out.primaryAccounts.assign(out.personCount, entity::Key{});

  for (const auto &[person, recordIx] : plan.primaryAcctRecordIx) {
    const auto personIx = static_cast<std::size_t>(person) - 1;

    if (personIx >= out.primaryAccounts.size()) {
      continue;
    }

    if (recordIx >= registry.records.size()) {
      continue;
    }

    out.primaryAccounts[personIx] = registry.records[recordIx].id;
  }

  out.paydayStorage = blueprints::buildPaydaysByPerson(
      baseTxns, registry, lookup, plan.startDate, plan.days, out.personCount);

  out.paydaySets.reserve(out.personCount);

  for (const auto &days : out.paydayStorage) {
    out.paydaySets.push_back(plPop::PaydaySet{
        .days = std::span<const std::uint32_t>(days.data(), days.size()),
    });
  }

  return out;
}

// ---------------------------------------------------------------------------
// Cards lookup
// ---------------------------------------------------------------------------

[[nodiscard]] plMarket::Cards
buildSpendingCards(const entity::card::Registry *creditCards,
                   std::uint32_t personCount) {
  plMarket::Cards cards(static_cast<std::size_t>(personCount));

  if (creditCards == nullptr || creditCards->records.empty()) {
    return cards;
  }

  for (const auto &record : creditCards->records) {
    if (record.owner == entity::invalidPerson) {
      continue;
    }

    if (record.owner == 0 || record.owner > personCount) {
      continue;
    }

    cards.assign(record.owner, record.key);
  }

  return cards;
}

// ---------------------------------------------------------------------------
// Bootstrap inputs assembly
// ---------------------------------------------------------------------------

[[nodiscard]] plMarket::BootstrapInputs assembleBootstrapInputs(
    const SpendingRunRequest &request, const blueprints::LegitBuildPlan &plan,
    const CensusScratch &scratch, const SpendingHabits &habits) {
  if (plan.personas.pack == nullptr) {
    throw std::invalid_argument(
        "spending routine requires a populated PersonaPlan.pack");
  }

  if (plan.days < 0) {
    throw std::invalid_argument("spending routine requires non-negative days");
  }

  plMarket::BootstrapInputs inputs;

  inputs.bounds.startDate = plan.startDate;
  inputs.bounds.days = static_cast<std::uint32_t>(plan.days);
  inputs.baseSeed = plan.seed;

  inputs.census.count = scratch.personCount;

  inputs.census.primaryAccounts = std::span<const entity::Key>(
      scratch.primaryAccounts.data(), scratch.primaryAccounts.size());

  inputs.census.personaTypes = std::span<const personas::Type>(
      plan.personas.pack->assignment.byPerson.data(),
      plan.personas.pack->assignment.byPerson.size());

  inputs.census.personaObjects = std::span<const entity::behavior::Persona>(
      plan.personas.pack->table.byPerson.data(),
      plan.personas.pack->table.byPerson.size());

  inputs.census.paydays = std::span<const plPop::PaydaySet>(
      scratch.paydaySets.data(), scratch.paydaySets.size());

  inputs.network.catalog = request.merchants;
  inputs.network.social = nullptr;

  inputs.cards = buildSpendingCards(request.creditCards, scratch.personCount);

  inputs.picking = habits.picking;
  inputs.exploration = habits.exploration;
  inputs.burst = habits.burst;

  return inputs;
}

} // namespace

std::vector<transactions::Transaction>
SpendingRoutine::run(const SpendingRunRequest &run,
                     SpendingLedgerSeed seed) const {
  const auto &request = run;
  const auto &plan = run.plan;
  const auto &registry = run.registry;

  if (request.rng == nullptr) {
    throw std::invalid_argument("spending routine requires a non-null rng");
  }

  if (request.txf == nullptr) {
    throw std::invalid_argument(
        "spending routine requires a non-null transaction factory");
  }

  if (request.accountsLookup == nullptr) {
    throw std::invalid_argument(
        "spending routine requires a non-null accountsLookup");
  }

  const auto scratch = buildCensusScratch(plan, *request.accountsLookup,
                                          registry, seed.baseTxns);

  auto inputs = assembleBootstrapInputs(request, plan, scratch, habits_);

  auto market = plMarket::buildMarket(std::move(inputs));

  random::RngFactory rngFactory{plan.seed};

  plSimulator::Engine engine{
      .rng = request.rng,
      .factory = request.txf,
      .ledger = seed.screenBook,
      .rngFactory = &rngFactory,
      .threadCount = 1,
  };

  std::vector<double> monthlyBurdens;

  if (request.portfolios != nullptr) {
    monthlyBurdens = ledger::buildMonthlyBurdens(
        *request.portfolios, scratch.personCount, plan.startDate);
  }

  plObligations::Snapshot obligations{
      .baseTxns = seed.baseTxns,
      .baseTxnsSorted = seed.baseTxnsSorted,
      .burden = plObligations::Burden(std::move(monthlyBurdens)),
  };

  plSimulator::RunPlanner planner{
      planning_.load,
      planning_.channels,
      planning_.paymentRules,
  };

  plSimulator::DayDriver dayDriver{
      plSimulator::DaySource{day_.variation, day_.seasonal},
      plSimulator::CommerceEvolver{dynamics_.commerce},
      plSimulator::PopulationDynamics{dynamics_.population},
      plSimulator::SpenderEmissionDriver{plSimulator::EmissionBehavior{
          .baseExploreP = emission_.baseExploreP,
          .exploration = emission_.exploration,
          .liquidity = emission_.liquidity,
      }},
  };

  plSimulator::Simulator simulator(market, engine, obligations,
                                   std::move(planner), std::move(dayDriver));

  return simulator.run();
}

std::vector<transactions::Transaction>
generateDayToDayTxns(const SpendingRunRequest &run, SpendingLedgerSeed seed) {
  return SpendingRoutine{}.run(run, seed);
}

} // namespace PhantomLedger::transfers::legit::routines::spending
