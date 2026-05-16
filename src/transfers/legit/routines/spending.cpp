#include "phantomledger/transfers/legit/routines/spending.hpp"

#include "phantomledger/activity/spending/market/bootstrap.hpp"
#include "phantomledger/activity/spending/market/cards.hpp"
#include "phantomledger/activity/spending/market/commerce/network.hpp"
#include "phantomledger/activity/spending/obligations/burden.hpp"
#include "phantomledger/activity/spending/simulator/commerce_evolver.hpp"
#include "phantomledger/activity/spending/simulator/day_driver.hpp"
#include "phantomledger/activity/spending/simulator/day_source.hpp"
#include "phantomledger/activity/spending/simulator/driver.hpp"
#include "phantomledger/activity/spending/simulator/population_dynamics.hpp"
#include "phantomledger/activity/spending/simulator/run_planner.hpp"
#include "phantomledger/activity/spending/simulator/spender_emission_driver.hpp"
#include "phantomledger/activity/spending/simulator/thread_runner.hpp"
#include "phantomledger/entities/synth/personas/pack.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/transfers/channels/credit_cards/card_cycle_driver.hpp"
#include "phantomledger/transfers/legit/blueprints/paydays.hpp"
#include "phantomledger/transfers/legit/ledger/burdens.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace PhantomLedger::transfers::legit::routines::spending {

namespace plSpending = ::PhantomLedger::activity::spending;
namespace plMarket = plSpending::market;
namespace plPop = plMarket::population;
namespace plObligations = plSpending::obligations;
namespace plSimulator = plSpending::simulator;
namespace plCredit = ::PhantomLedger::transfers::credit_cards;

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

SpendingRoutine &SpendingRoutine::cardLifecycle(CardLifecycleConfig value) {
  cards_ = std::move(value);
  return *this;
}

namespace {

[[nodiscard]] std::uint32_t
personCount(const blueprints::LegitBlueprint &plan) {
  if (plan.personas().pack == nullptr) {
    throw std::invalid_argument(
        "spending routine requires a populated PersonaPlan.pack");
  }

  return static_cast<std::uint32_t>(
      plan.personas().pack->table.byPerson.size());
}

struct CensusScratch {
  std::uint32_t personCount = 0;
  std::vector<entity::Key> primaryAccounts;
  std::vector<plPop::PaydaySet> paydaySets;

  std::vector<std::vector<std::uint32_t>> paydayStorage;
};

[[nodiscard]] CensusScratch
buildCensusScratch(const blueprints::LegitBlueprint &plan,
                   const entity::account::Lookup &lookup,
                   const entity::account::Registry &registry,
                   std::span<const transactions::Transaction> baseTxns) {
  CensusScratch out;

  out.personCount = personCount(plan);
  out.primaryAccounts.assign(out.personCount, entity::Key{});

  for (const auto &[person, recordIx] : plan.primaryAcctRecordIx()) {
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
      baseTxns,
      blueprints::LegitAccountIndex{.registry = &registry, .lookup = &lookup},
      time::Window{.start = plan.startDate(), .days = plan.days()},
      out.personCount);

  out.paydaySets.reserve(out.personCount);

  for (const auto &days : out.paydayStorage) {
    out.paydaySets.push_back(plPop::PaydaySet{
        .days = std::span<const std::uint32_t>(days.data(), days.size()),
    });
  }

  return out;
}

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

[[nodiscard]] plMarket::MarketSources
assembleMarketSources(const SpendingRoutine::PayeeDirectory &payees,
                      const blueprints::LegitBlueprint &plan,
                      const CensusScratch &scratch) {
  if (plan.days() < 0) {
    throw std::invalid_argument("spending routine requires non-negative days");
  }

  plMarket::MarketSources sources;

  sources.bounds.startDate = plan.startDate();
  sources.bounds.days = static_cast<std::uint32_t>(plan.days());
  sources.baseSeed = plan.seed();

  sources.census.count = scratch.personCount;

  sources.census.primaryAccounts = std::span<const entity::Key>(
      scratch.primaryAccounts.data(), scratch.primaryAccounts.size());

  sources.census.personaTypes = std::span<const personas::Type>(
      plan.personas().pack->assignment.byPerson.data(),
      plan.personas().pack->assignment.byPerson.size());

  sources.census.personaObjects = std::span<const entity::behavior::Persona>(
      plan.personas().pack->table.byPerson.data(),
      plan.personas().pack->table.byPerson.size());

  sources.census.paydays = std::span<const plPop::PaydaySet>(
      scratch.paydaySets.data(), scratch.paydaySets.size());

  sources.network.catalog = payees.merchants;
  sources.network.social = nullptr;

  sources.cards = buildSpendingCards(payees.creditCards, scratch.personCount);

  return sources;
}

[[nodiscard]] plMarket::PayeeSelectionRules
marketPayeesFrom(const SpendingHabits &habits) {
  plMarket::PayeeSelectionRules payees;
  payees.picking = habits.picking;
  return payees;
}

[[nodiscard]] plMarket::ShopperBehaviorRules
marketBehaviorFrom(const SpendingHabits &habits) {
  return plMarket::ShopperBehaviorRules{
      .burst = habits.burst,
      .exploration = habits.exploration,
  };
}

[[nodiscard]] plSimulator::RunPlanner plannerFrom(const RunPlanning &planning) {
  return plSimulator::RunPlanner{
      planning.load,
      planning.channels,
      planning.paymentRules,
  };
}

[[nodiscard]] plSimulator::DayDriver
dayDriverFrom(const DayPattern &day, const DynamicsProfile &dynamics,
              const EmissionProfile &emission) {
  return plSimulator::DayDriver{
      plSimulator::DaySource{day.variation, day.seasonal},
      plSimulator::CommerceEvolver{dynamics.commerce},
      plSimulator::PopulationDynamics{dynamics.population},
      plSimulator::SpenderEmissionDriver{
          plSimulator::SpenderEmissionDriver::Behavior{
              .baseExploreP = emission.baseExploreP,
              .exploration = emission.exploration,
              .liquidity = emission.liquidity,
              .rates = emission.rates,
          }},
  };
}

} // namespace

plMarket::Market SpendingRoutine::prepareMarket(
    const CensusSource &census, PayeeDirectory payees,
    std::span<const transactions::Transaction> baseTxns) const {
  const auto &plan = census.blueprint;
  const auto &registry = census.accounts.registry;

  const auto scratch =
      buildCensusScratch(plan, census.accounts.lookup, registry, baseTxns);

  auto sources = assembleMarketSources(payees, plan, scratch);
  const auto payeeRules = marketPayeesFrom(habits_);
  const auto behavior = marketBehaviorFrom(habits_);

  return plMarket::buildMarket(std::move(sources), payeeRules, behavior);
}

plObligations::Snapshot SpendingRoutine::prepareObligations(
    const CensusSource &census, ObligationSource obligations,
    std::span<const transactions::Transaction> baseTxns, bool baseTxnsSorted) {
  const auto &plan = census.blueprint;

  std::vector<double> monthlyBurdens;

  if (obligations.portfolios != nullptr) {
    monthlyBurdens = ledger::buildMonthlyBurdens(
        *obligations.portfolios, personCount(plan), plan.startDate());
  }

  return plObligations::Snapshot{
      .baseTxns = baseTxns,
      .baseTxnsSorted = baseTxnsSorted,
      .burden = plObligations::Burden(std::move(monthlyBurdens)),
  };
}

std::vector<transactions::Transaction>
SpendingRoutine::run(Execution execution, plMarket::Market &market,
                     const plObligations::Snapshot &obligations,
                     clearing::Ledger *screenBook) const {
  random::RngFactory rngFactory{execution.seed};

  const plSimulator::SpenderEmissionDriver::Threads emissionThreads{
      .rngFactory = &rngFactory,
      .count = plSimulator::resolveThreadCount(),
  };

  std::optional<plCredit::CardCycleDriver> cardDriver;
  if (cards_.active()) {
    plCredit::DriverInputs inputs{
        .cards = cards_.cards,
        .primaryAccounts = &cards_.primaryAccounts,
        .issuerAccount = cards_.issuerAccount,
        .window = cards_.window,
    };
    cardDriver.emplace(
        *cards_.rules, execution.txf,
        random::RngFactory{cards_.seed != 0 ? cards_.seed : execution.seed},
        inputs, screenBook);
  }

  plSimulator::Simulator simulator(market, execution.rng, execution.txf,
                                   obligations);

  simulator.ledger(screenBook)
      .planner(plannerFrom(planning_))
      .dayDriver(dayDriverFrom(day_, dynamics_, emission_))
      .emissionThreads(emissionThreads)
      .cardCycleDriver(cardDriver ? &*cardDriver : nullptr);

  return simulator.run();
}

} // namespace PhantomLedger::transfers::legit::routines::spending
