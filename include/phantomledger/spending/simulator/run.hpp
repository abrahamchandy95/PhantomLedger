#pragma once

#include "phantomledger/math/seasonal.hpp"
#include "phantomledger/spending/config/burst.hpp"
#include "phantomledger/spending/config/exploration.hpp"
#include "phantomledger/spending/config/liquidity.hpp"
#include "phantomledger/spending/config/picking.hpp"
#include "phantomledger/spending/dynamics/config.hpp"
#include "phantomledger/spending/market/market.hpp"
#include "phantomledger/spending/obligations/snapshot.hpp"
#include "phantomledger/spending/simulator/engine.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::spending::simulator {

struct RunInputs {
  // World-config-derived scalars that the simulator threads into
  // its plan. Caller pulls these from common::config::Events and
  // common::config::Merchants and hands them in.
  double txnsPerMonth = 0.0;
  double channelMerchantP = 0.0;
  double channelBillsP = 0.0;
  double channelP2pP = 0.0;
  double unknownOutflowP = 0.0;
  double baseExploreP = 0.0;
  double dayShockShape = 1.0;
  double preferBillersP = 0.85;
  std::uint32_t personDailyLimit = 0; // 0 = no cap
};

[[nodiscard]] std::vector<transactions::Transaction> simulate(
    const market::Market &market, Engine &engine,
    const obligations::Snapshot &obligations, const RunInputs &inputs,
    const dynamics::Config &dynamicsCfg = dynamics::kDefaultConfig,
    const config::BurstBehavior &burst = config::kDefaultBurst,
    const config::ExplorationHabits &exploration = config::kDefaultExploration,
    const config::LiquidityConstraints &liquidity =
        config::kDefaultLiquidityConstraints,
    const config::MerchantPickRules &picking = config::kDefaultPickRules,
    const math::seasonal::Config &seasonal = math::seasonal::kDefaultConfig);

} // namespace PhantomLedger::spending::simulator
