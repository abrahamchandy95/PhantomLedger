#include "phantomledger/activity/spending/simulator/run.hpp"

#include "phantomledger/activity/spending/simulator/driver.hpp"

namespace PhantomLedger::spending::simulator {

std::vector<transactions::Transaction>
simulate(market::Market &market, random::Rng &rng,
         const transactions::Factory &factory,
         const obligations::Snapshot &obligations) {
  Simulator simulator(market, rng, factory, obligations);
  return simulator.run();
}

} // namespace PhantomLedger::spending::simulator
