#include "phantomledger/spending/simulator/driver.hpp"

#include "phantomledger/spending/simulator/state.hpp"
#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace PhantomLedger::spending::simulator {
namespace {

constexpr double kTxnReserveSlack = 1.05;

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

std::vector<transactions::Transaction> Simulator::run() {
  if (market_.bounds().days == 0) {
    return {};
  }

  dayDriver_.resetFor(market_);
  dayDriver_.bindMarket(market_);
  dayDriver_.bindRng(rng_);
  dayDriver_.bindLedger(ledger_);
  dayDriver_.bindFactory(factory_);
  dayDriver_.emissionThreads(emissionThreads_);
  dayDriver_.prepareEmission(planner_.txnsPerMonth());

  PreparedRun run = planner_.build(market_, obligations_, ledger_,
                                   dayDriver_.sensitivities());

  dayDriver_.bindEmission(run.budget(), run.routing());

  const std::size_t reserveCapacity =
      !emissionThreads_.parallel()
          ? static_cast<std::size_t>(run.budget().targetTotalTxns *
                                     kTxnReserveSlack)
          : 0;

  RunState state(market_.population().count(), reserveCapacity,
                 run.budget().totalPersonDays, run.budget().targetTotalTxns);

  for (std::uint32_t dayIndex = 0; dayIndex < market_.bounds().days;
       ++dayIndex) {
    dayDriver_.runDay(run, state, dayIndex);
  }

  dayDriver_.finish(state);

  auto txns = std::move(state.txns());
  sortChronological(txns);
  return txns;
}

} // namespace PhantomLedger::spending::simulator
