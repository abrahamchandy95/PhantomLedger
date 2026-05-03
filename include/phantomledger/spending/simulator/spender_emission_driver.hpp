#pragma once

#include "phantomledger/primitives/concurrent/account_lock_array.hpp"
#include "phantomledger/spending/actors/day.hpp"
#include "phantomledger/spending/actors/explore.hpp"
#include "phantomledger/spending/liquidity/multiplier.hpp"
#include "phantomledger/spending/market/market.hpp"
#include "phantomledger/spending/simulator/engine.hpp"
#include "phantomledger/spending/simulator/prepared_run.hpp"
#include "phantomledger/spending/simulator/state.hpp"

#include <span>
#include <vector>

namespace PhantomLedger::spending::simulator {

class SpenderEmissionDriver {
public:
  struct Behavior {
    double baseExploreP = 0.0;
    actors::ExploreModifiers exploration{};
    liquidity::Throttle liquidity{};
  };

  SpenderEmissionDriver() = default;
  explicit SpenderEmissionDriver(Behavior behavior);

  SpenderEmissionDriver(const SpenderEmissionDriver &) = delete;
  SpenderEmissionDriver &operator=(const SpenderEmissionDriver &) = delete;
  SpenderEmissionDriver(SpenderEmissionDriver &&) noexcept = default;
  SpenderEmissionDriver &operator=(SpenderEmissionDriver &&) noexcept = default;

  void prepare(const market::Market &market, const RunResources &resources,
               double txnsPerMonth);

  void emitDay(const market::Market &market, const RunResources &resources,
               const PreparedRun::Population &population,
               const PreparedRun::Budget &budget,
               const PreparedRun::Routing &routingSnapshot, RunState &state,
               const actors::DayFrame &frame,
               std::span<const double> dailyMultipliers);

  void finish(RunState &state);

private:
  void prepareThreadStates(const market::Market &market,
                           const RunResources &resources, double txnsPerMonth);
  void prepareLockArray(const RunResources &resources);
  void mergeThreadTxns(RunState &state);

  Behavior behavior_{};
  std::vector<ThreadLocalState> threadStates_{};
  primitives::concurrent::AccountLockArray lockArray_{};
};

} // namespace PhantomLedger::spending::simulator
