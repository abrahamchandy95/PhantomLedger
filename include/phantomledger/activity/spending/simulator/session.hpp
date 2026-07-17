#pragma once

#include "phantomledger/activity/spending/market/market.hpp"
#include "phantomledger/activity/spending/obligations/snapshot.hpp"
#include "phantomledger/activity/spending/simulator/day_driver.hpp"
#include "phantomledger/activity/spending/simulator/prepared_run.hpp"
#include "phantomledger/activity/spending/simulator/run_planner.hpp"
#include "phantomledger/activity/spending/simulator/spender_emission_driver.hpp"
#include "phantomledger/activity/spending/simulator/state.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/channels/credit_cards/card_cycle_driver.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace PhantomLedger::activity::spending::simulator {

struct WindowOutput {
  // Days advanced by this call. finish() returns a zero-day advanced window
  // at the end of the market range.
  time::Window advancedWindow{};

  // Timestamp interval now proven complete. These intervals are contiguous
  // across outputs and collectively cover the full market range after
  // finish().
  time::Window finalizedWindow{};

  // Canonically sorted rows with timestamps in finalizedWindow.
  std::vector<transactions::Transaction> txns;
};

class Session {
public:
  Session(market::Market &market, random::Rng &rng,
          const transactions::Factory &factory,
          const obligations::Snapshot &obligations);

  Session(const Session &) = delete;
  Session &operator=(const Session &) = delete;
  Session(Session &&) = delete;
  Session &operator=(Session &&) = delete;

  Session &ledger(clearing::Ledger *ledger) noexcept;
  Session &planner(RunPlanner planner) noexcept;
  Session &dayDriver(DayDriver dayDriver) noexcept;
  Session &emissionThreads(SpenderEmissionDriver::Threads threads) noexcept;
  Session &
  cardCycleDriver(::PhantomLedger::transfers::credit_cards::CardCycleDriver
                      *cards) noexcept;

  [[nodiscard]] WindowOutput advance(time::Window window);

  // Requires the full market range to have been advanced. Performs
  // end-of-run emission and residual card-cycle work, then finalizes every
  // remaining row through market.bounds().end.
  [[nodiscard]] WindowOutput finish();

  [[nodiscard]] std::uint32_t nextDayIndex() const noexcept {
    return nextDayIndex_;
  }

  [[nodiscard]] bool finished() const noexcept { return finished_; }

  [[nodiscard]] const PreparedRun &prepared() const;

  [[nodiscard]] std::uint64_t cardEventCount() const noexcept {
    return cardEventCount_;
  }

private:
  // A future statement close can first reveal a backdated dispute for a
  // purchase in the preceding statement cycle. Monthly closes are at most
  // 31 days apart; 32 days is a conservative day-aligned finalization lag.
  static constexpr int kCardFinalizationLagDays = 32;

  void prepareOnce();
  [[nodiscard]] std::uint32_t dayIndexOf(time::TimePoint tp) const;

  void collectNewRows(std::vector<transactions::Transaction> rows);

  [[nodiscard]] WindowOutput drainFinalized(time::Window advancedWindow,
                                            time::TimePoint finalizedBoundExcl);

  market::Market &market_;
  random::Rng &rng_;
  const transactions::Factory &factory_;
  const obligations::Snapshot &obligations_;

  clearing::Ledger *ledger_ = nullptr;
  SpenderEmissionDriver::Threads emissionThreads_{};
  ::PhantomLedger::transfers::credit_cards::CardCycleDriver *cards_ = nullptr;

  RunPlanner planner_{};
  DayDriver dayDriver_{};

  std::optional<PreparedRun> prepared_;
  std::optional<RunState> state_;

  // Sorted rows not yet behind the card-safe watermark. This remains
  // bounded by the finalization lag plus future-dated lifecycle events.
  std::vector<transactions::Transaction> pendingTxns_;
  time::TimePoint finalizedThrough_{};

  std::uint32_t nextDayIndex_ = 0;
  std::uint64_t cardEventCount_ = 0;
  double reservePerDay_ = 0.0;
  bool preparedOnce_ = false;
  bool finished_ = false;
};

} // namespace PhantomLedger::activity::spending::simulator
