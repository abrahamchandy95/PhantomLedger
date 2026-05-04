#include "phantomledger/spending/simulator/run.hpp"

#include "phantomledger/spending/simulator/driver.hpp"

#include <utility>

namespace PhantomLedger::spending::simulator {

std::vector<transactions::Transaction>
simulate(market::Market &market, random::Rng &rng,
         const transactions::Factory &factory,
         const obligations::Snapshot &obligations, clearing::Ledger *ledger,
         RunPlanner planner, DayDriver dayDriver,
         SpenderEmissionDriver::Threads emissionThreads) {
  Simulator sim(market, rng, factory, obligations, ledger, std::move(planner),
                std::move(dayDriver), emissionThreads);
  return sim.run();
}

} // namespace PhantomLedger::spending::simulator
