#include "phantomledger/transfers/legit/routines/spending.hpp"

#include "phantomledger/entities/synth/personas/pack.hpp"
#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/spending/market/bootstrap.hpp"
#include "phantomledger/spending/market/cards.hpp"
#include "phantomledger/spending/market/commerce/network.hpp"
#include "phantomledger/spending/obligations/burden.hpp"
#include "phantomledger/spending/obligations/snapshot.hpp"
#include "phantomledger/spending/simulator/commerce_evolver.hpp"
#include "phantomledger/spending/simulator/day_driver.hpp"
#include "phantomledger/spending/simulator/day_source.hpp"
#include "phantomledger/spending/simulator/driver.hpp"
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
buildCensusScratch(const blueprints::LegitBlueprint &plan,
                   const entity::account::Lookup &lookup,
                   const entity::account::Registry &registry,
                   std::span<const transactions::Transaction> baseTxns) {
  if (plan.personas().pack == nullptr) {
    throw std::invalid_argument(
        "spending routine requires a populated PersonaPlan.pack");
  }

  CensusScratch out;

  out.personCount =
      static_cast<std::uint32_t>(plan.personas().pack->table.byPerson.size());

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
      baseTxns, registry, lookup, plan.startDate(), plan.days(),
      out.personCount);

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
// Market bootstrap assembly
// ---------------------------------------------------------------------------

[[nodiscard]] plMarket::MarketSources
assembleMarketSources(const SpendingRoutine::PayeeDirectory &payees,
                      const blueprints::LegitBlueprint &plan,
                      const CensusScratch &scratch) {
  if (plan.personas().pack == nullptr) {
    throw std::invalid_argument(
        "spending routine requires a populated PersonaPlan.pack");
  }

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

} // namespace

std::vector<transactions::Transaction>
SpendingRoutine::run(Execution execution, const CensusSource &census,
                     PayeeDirectory payees, ObligationSource obligations,
                     LedgerReplay replay) const {
  const auto &plan = census.blueprint;
  const auto &registry = census.accounts.registry;

  const auto scratch = buildCensusScratch(plan, census.accounts.lookup,
                                          registry, replay.baseTxns);

  auto sources = assembleMarketSources(payees, plan, scratch);
  const auto payeeRules = marketPayeesFrom(habits_);
  const auto behavior = marketBehaviorFrom(habits_);

  auto market = plMarket::buildMarket(std::move(sources), payeeRules, behavior);

  random::RngFactory rngFactory{plan.seed()};

  std::vector<double> monthlyBurdens;

  if (obligations.portfolios != nullptr) {
    monthlyBurdens = ledger::buildMonthlyBurdens(
        *obligations.portfolios, scratch.personCount, plan.startDate());
  }

  plObligations::Snapshot obligationSnapshot{
      .baseTxns = replay.baseTxns,
      .baseTxnsSorted = replay.baseTxnsSorted,
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
      plSimulator::SpenderEmissionDriver{
          plSimulator::SpenderEmissionDriver::Behavior{
              .baseExploreP = emission_.baseExploreP,
              .exploration = emission_.exploration,
              .liquidity = emission_.liquidity,
          }},
  };

  const plSimulator::SpenderEmissionDriver::Threads emissionThreads{
      .rngFactory = &rngFactory,
      .count = 1,
  };

  plSimulator::Simulator simulator(market, execution.rng, execution.txf,
                                   obligationSnapshot);
  simulator.ledger(replay.screenBook)
      .planner(std::move(planner))
      .dayDriver(std::move(dayDriver))
      .emissionThreads(emissionThreads);

  return simulator.run();
}

std::vector<transactions::Transaction>
generateDayToDayTxns(SpendingRoutine::Execution execution,
                     const SpendingRoutine::CensusSource &census,
                     SpendingRoutine::PayeeDirectory payees,
                     SpendingRoutine::ObligationSource obligations,
                     SpendingRoutine::LedgerReplay replay) {
  return SpendingRoutine{}.run(execution, census, payees, obligations, replay);
}

} // namespace PhantomLedger::transfers::legit::routines::spending
