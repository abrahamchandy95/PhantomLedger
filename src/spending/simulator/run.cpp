#include "phantomledger/spending/simulator/run.hpp"

#include "phantomledger/spending/simulator/driver.hpp"

namespace PhantomLedger::spending::simulator {

std::vector<transactions::Transaction>
simulate(const market::Market &market, Engine &engine,
         const obligations::Snapshot &obligations, const RunInputs &inputs,
         const dynamics::Config &dynamicsCfg,
         const config::BurstBehavior &burst,
         const config::ExplorationHabits &exploration,
         const config::LiquidityConstraints &liquidity,
         const config::MerchantPickRules &picking,
         const math::seasonal::Config &seasonal) {
  Simulator sim(market, engine, obligations, dynamicsCfg, burst, exploration,
                liquidity, picking, seasonal, inputs.txnsPerMonth,
                inputs.channelMerchantP, inputs.channelBillsP,
                inputs.channelP2pP, inputs.unknownOutflowP, inputs.baseExploreP,
                inputs.dayShockShape, inputs.preferBillersP,
                inputs.personDailyLimit);
  return sim.run();
}

} // namespace PhantomLedger::spending::simulator
