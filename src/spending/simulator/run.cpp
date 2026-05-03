#include "phantomledger/spending/simulator/run.hpp"

#include "phantomledger/spending/simulator/driver.hpp"

#include <utility>

namespace PhantomLedger::spending::simulator {

std::vector<transactions::Transaction>
simulate(market::Market &market, RunResources &resources,
         const obligations::Snapshot &obligations, RunPlanner planner,
         DayDriver dayDriver) {
  Simulator sim(market, resources, obligations, std::move(planner),
                std::move(dayDriver));
  return sim.run();
}

} // namespace PhantomLedger::spending::simulator
