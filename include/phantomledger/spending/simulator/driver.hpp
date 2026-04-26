#pragma once

#include "phantomledger/math/seasonal.hpp"
#include "phantomledger/spending/config/burst.hpp"
#include "phantomledger/spending/config/exploration.hpp"
#include "phantomledger/spending/config/liquidity.hpp"
#include "phantomledger/spending/config/picking.hpp"
#include "phantomledger/spending/dynamics/config.hpp"
#include "phantomledger/spending/dynamics/population/advance.hpp"
#include "phantomledger/spending/market/market.hpp"
#include "phantomledger/spending/obligations/snapshot.hpp"
#include "phantomledger/spending/simulator/engine.hpp"
#include "phantomledger/spending/simulator/plan.hpp"
#include "phantomledger/spending/simulator/state.hpp"
#include "phantomledger/transactions/record.hpp"

#include <vector>

namespace PhantomLedger::spending::simulator {

class Simulator {
public:
  Simulator(const market::Market &market, Engine &engine,
            const obligations::Snapshot &obligations,
            const dynamics::Config &dynamicsCfg,
            const config::BurstBehavior &burst,
            const config::ExplorationHabits &exploration,
            const config::LiquidityConstraints &liquidity,
            const config::MerchantPickRules &picking,
            const math::seasonal::Config &seasonal, double txnsPerMonth,
            double channelMerchantP, double channelBillsP, double channelP2pP,
            double unknownOutflowP, double baseExploreP, double dayShockShape,
            double preferBillersP, std::uint32_t personDailyLimit);

  Simulator(const Simulator &) = delete;
  Simulator &operator=(const Simulator &) = delete;
  Simulator(Simulator &&) = default;
  Simulator &operator=(Simulator &&) = delete;

  [[nodiscard]] std::vector<transactions::Transaction> run();

private:
  RunPlan buildPlan() const;

  // References to externally-owned inputs.
  const market::Market &market_;
  Engine &engine_;
  const obligations::Snapshot &obligations_;
  const dynamics::Config &dynamicsCfg_;
  const config::BurstBehavior &burst_;
  const config::ExplorationHabits &exploration_;
  const config::LiquidityConstraints &liquidity_;
  const config::MerchantPickRules &picking_;
  const math::seasonal::Config &seasonal_;

  // Plan-build inputs flattened to scalars.
  double txnsPerMonth_;
  double channelMerchantP_;
  double channelBillsP_;
  double channelP2pP_;
  double unknownOutflowP_;
  double baseExploreP_;
  double dayShockShape_;
  double preferBillersP_;
  std::uint32_t personDailyLimit_;

  // Owned per-run buffers.
  dynamics::population::Cohort cohort_;
  std::vector<double> sensitivities_;
  std::vector<std::uint32_t> paydayPersonIndicesScratch_;
  std::vector<double> momentumMultBuffer_;
  std::vector<double> dormancyMultBuffer_;
  std::vector<double> paycheckMultBuffer_;
  std::vector<double> dailyMultBuffer_;
};

} // namespace PhantomLedger::spending::simulator
