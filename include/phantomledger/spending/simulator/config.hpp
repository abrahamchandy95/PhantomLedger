#pragma once

#include "phantomledger/math/seasonal.hpp"
#include "phantomledger/spending/config/burst.hpp"
#include "phantomledger/spending/config/exploration.hpp"
#include "phantomledger/spending/config/liquidity.hpp"
#include "phantomledger/spending/config/picking.hpp"
#include "phantomledger/spending/dynamics/config.hpp"

#include <cstdint>

namespace PhantomLedger::spending::simulator {

struct ChannelMix {
  double merchantP = 0.0;
  double billsP = 0.0;
  double p2pP = 0.0;
  double externalUnknownP = 0.0;
};

struct SimulatorConfig {
  double txnsPerMonth = 0.0;
  ChannelMix channels{};

  double baseExploreP = 0.0;
  double dayShockShape = 1.0;
  double preferBillersP = 0.85;
  std::uint32_t personDailyLimit = 0; // 0 = no cap

  dynamics::Config dynamics = dynamics::kDefaultConfig;
  config::BurstBehavior burst = config::kDefaultBurst;
  config::ExplorationHabits exploration = config::kDefaultExploration;
  config::LiquidityConstraints liquidity = config::kDefaultLiquidityConstraints;
  config::MerchantPickRules picking = config::kDefaultPickRules;
  math::seasonal::Config seasonal = math::seasonal::kDefaultConfig;
};

} // namespace PhantomLedger::spending::simulator
