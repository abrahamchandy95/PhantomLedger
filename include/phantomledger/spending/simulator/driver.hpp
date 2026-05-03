#pragma once

#include "phantomledger/spending/market/market.hpp"
#include "phantomledger/spending/obligations/snapshot.hpp"
#include "phantomledger/spending/simulator/day_driver.hpp"
#include "phantomledger/spending/simulator/engine.hpp"
#include "phantomledger/spending/simulator/run_planner.hpp"
#include "phantomledger/transactions/record.hpp"

#include <vector>

namespace PhantomLedger::spending::simulator {

class Simulator {
public:
  Simulator(market::Market &market, RunResources &resources,
            const obligations::Snapshot &obligations, RunPlanner planner = {},
            DayDriver dayDriver = DayDriver{});

  Simulator(const Simulator &) = delete;
  Simulator &operator=(const Simulator &) = delete;
  Simulator(Simulator &&) noexcept = default;
  Simulator &operator=(Simulator &&) = delete;

  [[nodiscard]] std::vector<transactions::Transaction> run();

private:
  market::Market &market_;
  RunResources &resources_;
  const obligations::Snapshot &obligations_;

  RunPlanner planner_{};
  DayDriver dayDriver_{};
};

} // namespace PhantomLedger::spending::simulator
