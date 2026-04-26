#include "phantomledger/spending/simulator/run.hpp"

#include "phantomledger/spending/simulator/driver.hpp"

namespace PhantomLedger::spending::simulator {

std::vector<transactions::Transaction>
simulate(const market::Market &market, Engine &engine,
         const obligations::Snapshot &obligations,
         const SimulatorConfig &config) {
  Simulator sim(market, engine, obligations, config);
  return sim.run();
}

} // namespace PhantomLedger::spending::simulator
