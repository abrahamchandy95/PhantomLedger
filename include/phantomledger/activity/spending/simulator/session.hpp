#pragma once

#include "phantomledger/activity/spending/market/market.hpp"
#include "phantomledger/activity/spending/obligations/snapshot.hpp"
#include "phantomledger/activity/spending/simulator/card_cycle_billing.hpp"
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

#include <cstdint>
#include <optional>
#include <vector>

namespace PhantomLedger::activity::spending::simulator {

struct Batch {
  // Time simulated by this call. finish() returns a zero-day window.
  time::Window simulated{};

  // Timestamp interval now proven complete and safe from future back-dating.
  time::Window finalized{};

  // Canonically sorted transactions with timestamps inside the finalized
  // window.
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
  Session &cardCycleBilling(CardCycleBilling *cards) noexcept;

  [[nodiscard]] Batch advance(time::Window window);

  // Requires the full market range to have been advanced. Performs
  // end-of-run emission and residual card-cycle work, then finalizes every
  // remaining row through market.bounds().end.
  [[nodiscard]] Batch finish();

  [[nodiscard]] std::uint32_t nextDayIndex() const noexcept {
    return nextDayIndex_;
  }

  [[nodiscard]] bool isFinished() const noexcept { return isFinished_; }

  [[nodiscard]] const PreparedRun &prepared() const;

  [[nodiscard]] std::uint64_t cardEventCount() const noexcept {
    return cardEventCount_;
  }

private:
  // A future statement close can first reveal a backdated dispute for a
  // purchase in the preceding statement cycle. Monthly closes are at most
  // 31 days apart; 32 days is a conservative day-aligned finalization lag.
  static constexpr int kCardFinalizationLagDays = 32;

  void ensurePrepared();

  [[nodiscard]] std::uint32_t dayIndexFor(time::TimePoint tp) const;

  void bufferTransactions(std::vector<transactions::Transaction> rows);

  [[nodiscard]] Batch drainFinalized(time::Window simulated,
                                     time::TimePoint finalizedBoundExcl);

  market::Market &market_;
  random::Rng &rng_;
  const transactions::Factory &factory_;
  const obligations::Snapshot &obligations_;

  clearing::Ledger *ledger_ = nullptr;
  SpenderEmissionDriver::Threads emissionThreads_{};
  CardCycleBilling *cards_ = nullptr;

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

  bool isPrepared_ = false;
  bool isFinished_ = false;
};

} // namespace PhantomLedger::activity::spending::simulator
