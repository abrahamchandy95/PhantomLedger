#pragma once

#include "phantomledger/primitives/concurrent/account_lock_array.hpp"
#include "phantomledger/spending/actors/day.hpp"
#include "phantomledger/spending/dynamics/population/advance.hpp"
#include "phantomledger/spending/market/market.hpp"
#include "phantomledger/spending/obligations/snapshot.hpp"
#include "phantomledger/spending/simulator/config.hpp"
#include "phantomledger/spending/simulator/engine.hpp"
#include "phantomledger/spending/simulator/plan.hpp"
#include "phantomledger/spending/simulator/state.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::spending::simulator {

class Simulator {
public:
  Simulator(const market::Market &market, Engine &engine,
            const obligations::Snapshot &obligations,
            const SimulatorConfig &config);

  Simulator(const Simulator &) = delete;
  Simulator &operator=(const Simulator &) = delete;
  Simulator(Simulator &&) = default;
  Simulator &operator=(Simulator &&) = delete;

  [[nodiscard]] std::vector<transactions::Transaction> run();

private:
  [[nodiscard]] RunPlan buildPlan() const;

  void prepareThreadStates();
  void prepareLockArray();
  void mergeThreadTxns(RunState &state);

  void runDay(const RunPlan &plan, RunState &state, std::uint32_t dayIndex);

  void evolveCommerceIfNeeded(std::uint32_t dayIndex);

  [[nodiscard]] actors::DayFrame buildDayFrame(const RunPlan &plan,
                                               std::uint32_t dayIndex);

  void advanceLedgerToDay(const RunPlan &plan, RunState &state,
                          const actors::DayFrame &frame);

  [[nodiscard]] std::span<const std::uint32_t>
  updatePaydayState(const RunPlan &plan, RunState &state,
                    std::uint32_t dayIndex);

  void advanceDynamics(const RunPlan &plan,
                       std::span<const std::uint32_t> paydayPersons);

  void draftSpenderTransactions(const RunPlan &plan, RunState &state,
                                const actors::DayFrame &frame);

  const market::Market &market_;
  Engine &engine_;
  const obligations::Snapshot &obligations_;

  SimulatorConfig config_;

  dynamics::population::Cohort cohort_;
  std::vector<double> sensitivities_;
  std::vector<double> dailyMultBuffer_;

  std::vector<ThreadLocalState> threadStates_;
  primitives::concurrent::AccountLockArray lockArray_;
};

} // namespace PhantomLedger::spending::simulator
