#pragma once

#include "phantomledger/primitives/concurrent/account_lock_array.hpp"
#include "phantomledger/spending/actors/day.hpp"
#include "phantomledger/spending/config/burst.hpp"
#include "phantomledger/spending/config/exploration.hpp"
#include "phantomledger/spending/config/liquidity.hpp"
#include "phantomledger/spending/market/market.hpp"
#include "phantomledger/spending/simulator/engine.hpp"
#include "phantomledger/spending/simulator/plan.hpp"
#include "phantomledger/spending/simulator/state.hpp"

#include <span>
#include <vector>

namespace PhantomLedger::spending::simulator {

struct EmissionBehavior {
  double baseExploreP = 0.0;
  config::BurstBehavior burst = config::kDefaultBurst;
  config::ExplorationHabits exploration = config::kDefaultExploration;
  config::LiquidityConstraints liquidity = config::kDefaultLiquidityConstraints;
};

class SpenderEmissionDriver {
public:
  SpenderEmissionDriver() = default;
  explicit SpenderEmissionDriver(EmissionBehavior behavior);

  SpenderEmissionDriver(const SpenderEmissionDriver &) = delete;
  SpenderEmissionDriver &operator=(const SpenderEmissionDriver &) = delete;
  SpenderEmissionDriver(SpenderEmissionDriver &&) noexcept = default;
  SpenderEmissionDriver &operator=(SpenderEmissionDriver &&) noexcept = default;

  void prepare(const market::Market &market, const Engine &engine,
               const TransactionLoad &load);

  void emitDay(const market::Market &market, const Engine &engine,
               const RunPlan &plan, RunState &state,
               const actors::DayFrame &frame,
               std::span<const double> dailyMultipliers);

  void finish(RunState &state);

private:
  void prepareThreadStates(const market::Market &market, const Engine &engine,
                           const TransactionLoad &load);
  void prepareLockArray(const Engine &engine);
  void mergeThreadTxns(RunState &state);

  EmissionBehavior behavior_{};
  std::vector<ThreadLocalState> threadStates_{};
  primitives::concurrent::AccountLockArray lockArray_{};
};

} // namespace PhantomLedger::spending::simulator
