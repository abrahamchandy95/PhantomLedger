#include "phantomledger/transfers/legit/routines/spending.hpp"

#include "phantomledger/entities/synth/personas/pack.hpp"
#include "phantomledger/spending/market/bootstrap.hpp"
#include "phantomledger/spending/market/cards.hpp"
#include "phantomledger/spending/market/commerce/network.hpp"
#include "phantomledger/spending/obligations/burden.hpp"
#include "phantomledger/spending/obligations/snapshot.hpp"
#include "phantomledger/spending/simulator/driver.hpp"
#include "phantomledger/spending/simulator/engine.hpp"
#include "phantomledger/transfers/legit/blueprints/paydays.hpp"
#include "phantomledger/transfers/legit/ledger/burdens.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace PhantomLedger::transfers::legit::routines::spending {

namespace pl_spending = ::PhantomLedger::spending;
namespace pl_market = pl_spending::market;
namespace pl_pop = pl_market::population;
namespace pl_obligations = pl_spending::obligations;
namespace pl_simulator = pl_spending::simulator;

namespace {

// ---------------------------------------------------------------------------
// Census materialisation
// ---------------------------------------------------------------------------

struct CensusScratch {
  std::uint32_t personCount = 0;
  std::vector<entity::Key> primaryAccounts;
  std::vector<pl_pop::PaydaySet> paydaySets;

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
    out.paydaySets.push_back(pl_pop::PaydaySet{
        .days = std::span<const std::uint32_t>(days.data(), days.size()),
    });
  }

  return out;
}

// ---------------------------------------------------------------------------
// Cards lookup
// ---------------------------------------------------------------------------

[[nodiscard]] pl_market::Cards
buildSpendingCards(const blueprints::CCState &ccState,
                   std::uint32_t personCount) {
  pl_market::Cards cards(static_cast<std::size_t>(personCount));

  if (!ccState.enabled() || ccState.cards == nullptr) {
    return cards;
  }

  for (const auto &record : ccState.cards->records) {
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

[[nodiscard]] pl_market::BootstrapInputs
assembleBootstrapInputs(const blueprints::Blueprint &request,
                        const blueprints::LegitBuildPlan &plan,
                        const CensusScratch &scratch) {
  if (plan.personas.pack == nullptr) {
    throw std::invalid_argument(
        "spending routine requires a populated PersonaPlan.pack");
  }

  if (plan.days < 0) {
    throw std::invalid_argument("spending routine requires non-negative days");
  }

  pl_market::BootstrapInputs inputs;

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

  inputs.census.paydays = std::span<const pl_pop::PaydaySet>(
      scratch.paydaySets.data(), scratch.paydaySets.size());

  inputs.network.catalog = request.network.merchants;
  inputs.network.social = nullptr;

  inputs.cards = buildSpendingCards(request.ccState, scratch.personCount);

  return inputs;
}

} // namespace

std::vector<transactions::Transaction>
generateDayToDayTxns(const blueprints::Blueprint &request,
                     const blueprints::LegitBuildPlan &plan,
                     const entity::account::Ownership &ownership,
                     const entity::account::Registry &registry,
                     std::span<const transactions::Transaction> baseTxns,
                     clearing::Ledger *screenBook, bool baseTxnsSorted,
                     const pl_simulator::SimulatorConfig &cfg) {
  (void)ownership;

  if (request.timeline.rng == nullptr) {
    throw std::invalid_argument(
        "spending routine requires a non-null timeline.rng");
  }

  if (request.network.accountsLookup == nullptr) {
    throw std::invalid_argument(
        "spending routine requires a non-null accountsLookup");
  }

  const auto scratch = buildCensusScratch(plan, *request.network.accountsLookup,
                                          registry, baseTxns);

  auto inputs = assembleBootstrapInputs(request, plan, scratch);
  auto market = pl_market::buildMarket(std::move(inputs));

  random::RngFactory rngFactory{plan.seed};

  const transactions::Factory txf(*request.timeline.rng,
                                  request.overrides.infra);

  pl_simulator::Engine engine{
      .rng = request.timeline.rng,
      .factory = &txf,
      .ledger = screenBook,
      .rngFactory = &rngFactory,
      .threadCount = 1,
  };

  std::vector<double> monthlyBurdens;

  if (request.network.portfolios != nullptr) {
    monthlyBurdens = ledger::buildMonthlyBurdens(
        *request.network.portfolios, scratch.personCount, plan.startDate);
  }

  pl_obligations::Snapshot obligations{
      .baseTxns = baseTxns,
      .baseTxnsSorted = baseTxnsSorted,
      .burden = pl_obligations::Burden(std::move(monthlyBurdens)),
  };

  pl_simulator::Simulator simulator(market, engine, obligations, cfg);
  return simulator.run();
}

} // namespace PhantomLedger::transfers::legit::routines::spending
