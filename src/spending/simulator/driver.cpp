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

Simulator::Simulator(market::Market &market, Engine &engine,
                     const obligations::Snapshot &obligations,
                     RunPlanner planner, DayDriver dayDriver)
    : market_(market), engine_(engine), obligations_(obligations),
      planner_(std::move(planner)), dayDriver_(std::move(dayDriver)) {}

std::vector<transactions::Transaction> Simulator::run() {
  if (market_.bounds().days == 0) {
    return {};
  }

  dayDriver_.prepare(market_, engine_, planner_.load());

  RunPlan plan = planner_.build(market_, obligations_, engine_.ledger,
                                dayDriver_.sensitivities());

  const std::size_t reserveCapacity =
      engine_.threadCount <= 1
          ? static_cast<std::size_t>(plan.budget.targetTotalTxns *
                                     kTxnReserveSlack)
          : 0;

  RunState state(market_.population().count(), reserveCapacity,
                 plan.budget.totalPersonDays, plan.budget.targetTotalTxns);

  for (std::uint32_t dayIndex = 0; dayIndex < market_.bounds().days;
       ++dayIndex) {
    dayDriver_.runDay(market_, engine_, plan, state, dayIndex);
  }

  dayDriver_.finish(state);

  auto txns = std::move(state.txns());
  sortChronological(txns);
  return txns;
}

} // namespace PhantomLedger::spending::simulator
