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
                     const obligations::Snapshot &obligations,
                     clearing::Ledger *ledger, RunPlanner planner,
                     DayDriver dayDriver,
                     SpenderEmissionDriver::Threads emissionThreads)
    : market_(market), rng_(rng), factory_(factory), obligations_(obligations),
      ledger_(ledger), emissionThreads_(emissionThreads),
      planner_(std::move(planner)), dayDriver_(std::move(dayDriver)) {}

std::vector<transactions::Transaction> Simulator::run() {
  if (market_.bounds().days == 0) {
    return {};
  }

  dayDriver_.prepare(market_, rng_, factory_, ledger_, emissionThreads_,
                     planner_.txnsPerMonth());

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
    dayDriver_.runDay(market_, rng_, ledger_, run, state, dayIndex);
  }

  dayDriver_.finish(state);

  auto txns = std::move(state.txns());
  sortChronological(txns);
  return txns;
}

} // namespace PhantomLedger::spending::simulator
