#include "phantomledger/activity/spending/simulator/driver.hpp"

#include "phantomledger/activity/spending/diagnostics.hpp"
#include "phantomledger/activity/spending/simulator/state.hpp"
#include "phantomledger/activity/spending/simulator/warm_start.hpp"
#include "phantomledger/diagnostics/logger.hpp"
#include "phantomledger/diagnostics/memory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <utility>

namespace PhantomLedger::activity::spending::simulator {
namespace {

constexpr double kTxnReserveSlack = 1.05;

// Warn-only memory governor (owner directive: diagnostics, never new
// knobs or refusals): a retained-corpus reserve above this share of
// physical RAM risks swap thrashing, and the warning points at the
// windowed engine — which is the binary's production path already; the
// retained-corpus Simulator is the library-level reference the tests
// use as their oracle.
constexpr double kResidentBudgetShare = 0.70;

void sortChronological(std::vector<transactions::Transaction> &txns) {
  std::sort(
      txns.begin(), txns.end(),
      transactions::Comparator{transactions::Comparator::Scope::fundsTransfer});
}

} // namespace

Simulator::Simulator(market::Market &market, random::Rng &rng,
                     const transactions::Factory &factory,
                     const obligations::Snapshot &obligations)
    : market_(market), rng_(rng), factory_(factory), obligations_(obligations) {
}

Simulator &Simulator::ledger(clearing::Ledger *ledger) noexcept {
  ledger_ = ledger;
  return *this;
}

Simulator &Simulator::planner(RunPlanner planner) noexcept {
  planner_ = std::move(planner);
  return *this;
}

Simulator &Simulator::dayDriver(DayDriver dayDriver) noexcept {
  dayDriver_ = std::move(dayDriver);
  return *this;
}

Simulator &
Simulator::emissionThreads(SpenderEmissionDriver::Threads threads) noexcept {
  emissionThreads_ = threads;
  return *this;
}

Simulator &Simulator::cardCycleBilling(CardCycleBilling *cards) noexcept {
  cards_ = cards;
  return *this;
}

std::vector<transactions::Transaction> Simulator::run() {
  if (market_.bounds().days == 0) {
    return {};
  }

  PL_LOG_INFO(sim, "Simulator::run starting: days=%u population=%u threads=%u",
              market_.bounds().days, market_.population().count(),
              emissionThreads_.count);

  dayDriver_.resetFor(market_);
  dayDriver_.bindMarket(market_);
  dayDriver_.bindRng(rng_);
  dayDriver_.bindLedger(ledger_);
  dayDriver_.bindFactory(factory_);
  dayDriver_.bindCardCycleBilling(cards_);
  dayDriver_.emissionThreads(emissionThreads_);
  dayDriver_.prepareEmission(planner_.txnsPerMonth());

  PreparedRun run = planner_.build(market_, obligations_, ledger_,
                                   dayDriver_.sensitivities());

  PL_LOG_INFO(sim,
              "Plan built: targetTotalTxns=%.0f totalPersonDays=%llu "
              "txnsPerMonth=%.2f activeSpenders=%u",
              run.budget().targetTotalTxns,
              static_cast<unsigned long long>(run.budget().totalPersonDays),
              planner_.txnsPerMonth(), run.population().activeCount());

  dayDriver_.bindEmission(run.budget(), run.routing());

  const std::size_t reserveCapacity =
      static_cast<std::size_t>(run.budget().targetTotalTxns * kTxnReserveSlack);

  const double reserveMB =
      static_cast<double>(reserveCapacity) *
      static_cast<double>(sizeof(transactions::Transaction)) /
      (1024.0 * 1024.0);

  PL_LOG_INFO(mem,
              "pre-flight: retained-corpus reserve ~%.1f MB (%zu rows x "
              "%zu B/row, incl. 1.05 slack); the windowed engine bounds "
              "this instead of retaining it",
              reserveMB, reserveCapacity, sizeof(transactions::Transaction));

  const double physicalMB =
      ::PhantomLedger::diagnostics::memory::physicalRamMB();
  if (physicalMB > 0.0 && reserveMB > physicalMB * kResidentBudgetShare) {
    PL_LOG_WARN(mem,
                "retained-corpus reserve ~%.0f MB exceeds %.0f%% of physical "
                "RAM (%.0f MB): this retained-corpus run risks swap "
                "thrashing — use the windowed engine, which bounds staging "
                "and streams the corpus instead of retaining it",
                reserveMB, kResidentBudgetShare * 100.0, physicalMB);
  }

  RunState state(market_.population().count(), reserveCapacity,
                 run.budget().totalPersonDays, run.budget().targetTotalTxns);

  applyWarmStartDaysSincePayday(state, run.population().spenders);
  PL_LOG_DEBUG(sim, "warm-start applied for %zu spenders",
               run.population().spenders.size());

  auto &stats = diagnostics::Stats::instance();
  stats.reset();

  const auto runStartTs = std::chrono::steady_clock::now();

  for (std::uint32_t dayIndex = 0; dayIndex < market_.bounds().days;
       ++dayIndex) {
    const auto dayStartTs = std::chrono::steady_clock::now();
    dayDriver_.runDay(run, state, dayIndex);
    stats.recordDaySnapshot(dayIndex);
    const auto dayMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - dayStartTs)
                           .count();
    PL_LOG_DEBUG(sim, "day %u complete in %lldms (running total txns=%zu)",
                 dayIndex, static_cast<long long>(dayMs), state.txns().size());
  }

  dayDriver_.finish(state);

  auto cardResidual = dayDriver_.drainCardCycles();
  if (!cardResidual.empty()) {
    state.txns().reserve(state.txns().size() + cardResidual.size());
    for (auto &txn : cardResidual) {
      state.txns().push_back(std::move(txn));
    }
  }

  const auto runMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - runStartTs)
                         .count();
  PL_LOG_INFO(sim, "Simulator::run finished in %lldms; raw txns=%zu",
              static_cast<long long>(runMs), state.txns().size());

  stats.dump();

  auto txns = std::move(state.txns());
  sortChronological(txns);
  return txns;
}

} // namespace PhantomLedger::activity::spending::simulator
