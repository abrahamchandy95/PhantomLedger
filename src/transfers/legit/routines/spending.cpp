#include "phantomledger/transfers/legit/routines/spending.hpp"

#include "phantomledger/activity/spending/market/bootstrap.hpp"
#include "phantomledger/activity/spending/market/cards.hpp"
#include "phantomledger/activity/spending/market/commerce/network.hpp"
#include "phantomledger/activity/spending/obligations/burden.hpp"
#include "phantomledger/activity/spending/simulator/driver.hpp"
#include "phantomledger/activity/spending/simulator/spender_emission_driver.hpp"
#include "phantomledger/activity/spending/simulator/thread_runner.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/synth/personas/pack.hpp"
#include "phantomledger/synth/personas/timeline.hpp"
#include "phantomledger/transfers/channels/credit_cards/card_cycle_driver.hpp"
#include "phantomledger/transfers/legit/blueprints/paydays.hpp"
#include "phantomledger/transfers/legit/ledger/burdens.hpp"
#include "phantomledger/transfers/legit/routines/spending/simulator_wiring.hpp"

#include <algorithm>
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

  /* Every deposit account per person, PRIMARY FIRST, flattened with a
   * count + 1 offset array. The market's population View copies both. */
  std::vector<std::uint32_t> depositOffsets;
  std::vector<entity::Key> depositAccounts;

  std::vector<plPop::PaydaySet> paydaySets;

  std::vector<std::vector<std::uint32_t>> paydayStorage;

  // Per-person retirement + death day-index storage (see
  // buildEventDays).
  std::vector<std::uint32_t> retirementDays;
  std::vector<std::uint32_t> deathDays;
};

// The window day-indices of the retirement consumption step and of
// DEATH, from the blueprint pack's
// persona-timeline lane — BOTH engines ride the same blueprint, so the
// oracle and the windowed path carry identical values (unlike the
// homeAreas carrier there is no empty-on-oracle mode).
//
// Retirement sentinel (kNoRetirementDay) for: seed retirees and
// highNetWorth (a seed retiree's archetype already encodes
// retired-calibrated spending — the step models only the IN-WINDOW
// transition), claims at/after the window end, and packs without a
// timeline lane (defensive; the step then simply never binds).
//
// Death sentinel (kNoDeathDay) only for window survivors and
// carrier-less packs — death has NO persona exemptions.
struct EventDays {
  std::vector<std::uint32_t> retirement;
  std::vector<std::uint32_t> death;
};

[[nodiscard]] EventDays buildEventDays(const blueprints::LegitBlueprint &plan,
                                       std::uint32_t personCount) {
  EventDays out{
      .retirement =
          std::vector<std::uint32_t>(personCount, plPop::kNoRetirementDay),
      .death = std::vector<std::uint32_t>(personCount, plPop::kNoDeathDay),
  };

  const auto *pack = plan.personas().pack;
  if (pack == nullptr || pack->timelines.size() != personCount ||
      plan.days() <= 0) {
    return out;
  }

  const std::int64_t startEpoch = time::toEpochSeconds(plan.startDate());
  const std::int64_t endEpochExcl =
      startEpoch + static_cast<std::int64_t>(plan.days()) * 86'400;

  const auto dayIndexOf = [&](std::int64_t epoch) {
    return static_cast<std::uint32_t>(
        std::max<std::int64_t>(0, (epoch - startEpoch) / 86'400));
  };

  for (std::uint32_t i = 0; i < personCount; ++i) {
    const auto &tl = pack->timelines[i];

    if (tl.seed != personas::Type::retiree &&
        tl.seed != personas::Type::highNetWorth) {
      const std::int64_t claim = time::toEpochSeconds(tl.retirement);
      if (claim < endEpochExcl) {
        out.retirement[i] = dayIndexOf(claim);
      }
    }

    const std::int64_t death = time::toEpochSeconds(tl.death);
    if (death < endEpochExcl) {
      out.death[i] = dayIndexOf(death);
    }
  }

  return out;
}

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

  /* The card-view instrument set's DEPOSIT half. `ownedAccountSlices` walks
   * `ownership.byPersonIndex`, which already holds the primary at the front
   * of each person's slice — but slot 0 is written EXPLICITLY here rather
   * than inherited from that ordering, because `emitBill`, `emitExternal`
   * and `emitP2p` all source from slot 0 unconditionally and a reordering
   * upstream would silently move three whole channels.
   *
   * The role/bank filter keeps this to the person's own deposit accounts: a
   * proprietor's `Role::business` account is owned by them and appears in the
   * same slice, and routing a household card purchase out of it would invent
   * a relationship the world does not model. */
  out.depositOffsets.assign(static_cast<std::size_t>(out.personCount) + 1, 0);
  out.depositAccounts.reserve(out.personCount);

  for (std::uint32_t i = 0; i < out.personCount; ++i) {
    const auto primary = out.primaryAccounts[i];
    out.depositOffsets[i] =
        static_cast<std::uint32_t>(out.depositAccounts.size());

    if (!entity::valid(primary)) {
      continue;
    }
    out.depositAccounts.push_back(primary);

    const auto person = static_cast<entity::PersonId>(i + 1);
    for (const auto recordIx : plan.ownedAccountSlices().recordsFor(person)) {
      if (recordIx >= registry.records.size()) {
        continue;
      }
      const auto id = registry.records[recordIx].id;
      if (id == primary || id.role != primary.role || id.bank != primary.bank) {
        continue;
      }
      out.depositAccounts.push_back(id);
    }
  }

  out.depositOffsets[out.personCount] =
      static_cast<std::uint32_t>(out.depositAccounts.size());

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

  auto eventDays = buildEventDays(plan, out.personCount);
  out.retirementDays = std::move(eventDays.retirement);
  out.deathDays = std::move(eventDays.death);

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

[[nodiscard]] plMarket::MarketSources assembleMarketSources(
    const SpendingRoutine::PayeeDirectory &payees,
    const blueprints::LegitBlueprint &plan, const CensusScratch &scratch,
    std::span<const ::PhantomLedger::entity::geography::GeoAreaId> homeAreas,
    const ::PhantomLedger::entity::parties::relocation::Schedule *relocation) {
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

  sources.census.depositOffsets = std::span<const std::uint32_t>(
      scratch.depositOffsets.data(), scratch.depositOffsets.size());

  sources.census.depositAccounts = std::span<const entity::Key>(
      scratch.depositAccounts.data(), scratch.depositAccounts.size());

  sources.census.personaTypes = std::span<const personas::Type>(
      plan.personas().pack->assignment.byPerson.data(),
      plan.personas().pack->assignment.byPerson.size());

  sources.census.personaObjects = std::span<const entity::behavior::Persona>(
      plan.personas().pack->table.byPerson.data(),
      plan.personas().pack->table.byPerson.size());

  sources.census.paydays = std::span<const plPop::PaydaySet>(
      scratch.paydaySets.data(), scratch.paydaySets.size());

  /* The per-person home area reaches the market's population View here. */
  sources.census.homeAreas = homeAreas;

  /* The home-area HISTORY. The population View refreshes its snapshot from
   * this at each month boundary, and the geo-pool builder covers the union of
   * every area it reaches. */
  sources.census.relocation = relocation;

  // The retirement + death day-indices, from the blueprint pack's
  // timeline lane (identical on both engines).
  sources.census.retirementDays = std::span<const std::uint32_t>(
      scratch.retirementDays.data(), scratch.retirementDays.size());
  sources.census.deathDays = std::span<const std::uint32_t>(
      scratch.deathDays.data(), scratch.deathDays.size());

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

plMarket::Market SpendingRoutine::prepareMarket(
    const CensusSource &census, PayeeDirectory payees,
    std::span<const transactions::Transaction> baseTxns) const {
  const auto &plan = census.blueprint;
  const auto &registry = census.accounts.registry;

  const auto scratch =
      buildCensusScratch(plan, census.accounts.lookup, registry, baseTxns);

  auto sources = assembleMarketSources(payees, plan, scratch, census.homeAreas,
                                       census.relocation);
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
        // Card servicing stops at account closure.
        .timelines = cards_.timelines,
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
      .cardCycleBilling(cardDriver ? &*cardDriver : nullptr);

  return simulator.run();
}

} // namespace PhantomLedger::transfers::legit::routines::spending
