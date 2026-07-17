#include "phantomledger/activity/spending/simulator/session.hpp"

#include "phantomledger/activity/spending/simulator/warm_start.hpp"
#include "phantomledger/diagnostics/logger.hpp"
#include "phantomledger/diagnostics/spending_stats.hpp"
#include "phantomledger/primitives/time/constants.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

namespace PhantomLedger::activity::spending::simulator {

namespace {

namespace diag = ::PhantomLedger::diagnostics;

constexpr double kTxnReserveSlack = 1.05;

[[nodiscard]] transactions::Comparator chronologicalComparator() noexcept {
  return transactions::Comparator{
      transactions::Comparator::Scope::fundsTransfer};
}

void sortChronological(std::vector<transactions::Transaction> &txns) {
  std::sort(txns.begin(), txns.end(), chronologicalComparator());
}

[[nodiscard]] int dayDistance(time::TimePoint start, time::TimePoint endExcl) {
  const auto seconds =
      time::toEpochSeconds(endExcl) - time::toEpochSeconds(start);
  if (seconds < 0 || seconds % time::kSecondsPerDay != 0) {
    throw std::logic_error("Session finalized watermark is not day-aligned");
  }
  return static_cast<int>(seconds / time::kSecondsPerDay);
}

} // namespace

Session::Session(market::Market &market, random::Rng &rng,
                 const transactions::Factory &factory,
                 const obligations::Snapshot &obligations)
    : market_(market), rng_(rng), factory_(factory), obligations_(obligations),
      finalizedThrough_(market.bounds().startDate) {}

Session &Session::ledger(clearing::Ledger *ledger) noexcept {
  ledger_ = ledger;
  return *this;
}

Session &Session::planner(RunPlanner planner) noexcept {
  planner_ = std::move(planner);
  return *this;
}

Session &Session::dayDriver(DayDriver dayDriver) noexcept {
  dayDriver_ = std::move(dayDriver);
  return *this;
}

Session &
Session::emissionThreads(SpenderEmissionDriver::Threads threads) noexcept {
  emissionThreads_ = threads;
  return *this;
}

Session &Session::cardCycleDriver(
    ::PhantomLedger::transfers::credit_cards::CardCycleDriver *cards) noexcept {
  cards_ = cards;
  return *this;
}

const PreparedRun &Session::prepared() const {
  if (!prepared_.has_value()) {
    throw std::logic_error("Session::prepared requires a prior advance()");
  }
  return *prepared_;
}

void Session::prepareOnce() {
  if (preparedOnce_) {
    return;
  }

  dayDriver_.resetFor(market_);
  dayDriver_.bindMarket(market_);
  dayDriver_.bindRng(rng_);
  dayDriver_.bindLedger(ledger_);
  dayDriver_.bindFactory(factory_);
  dayDriver_.bindCardCycleDriver(cards_);
  dayDriver_.emissionThreads(emissionThreads_);
  dayDriver_.prepareEmission(planner_.txnsPerMonth());

  prepared_ = planner_.build(market_, obligations_, ledger_,
                             dayDriver_.sensitivities());

  PL_LOG_INFO(
      sim,
      "Session plan built: targetTotalTxns=%.0f totalPersonDays=%llu "
      "activeSpenders=%u days=%u",
      prepared_->budget().targetTotalTxns,
      static_cast<unsigned long long>(prepared_->budget().totalPersonDays),
      prepared_->population().activeCount(), market_.bounds().days);

  dayDriver_.bindEmission(prepared_->budget(), prepared_->routing());

  const auto totalDays =
      static_cast<double>(std::max<std::uint32_t>(1, market_.bounds().days));
  reservePerDay_ =
      prepared_->budget().targetTotalTxns * kTxnReserveSlack / totalDays;

  state_.emplace(market_.population().count(),
                 static_cast<std::size_t>(reservePerDay_ * 31.0),
                 prepared_->budget().totalPersonDays,
                 prepared_->budget().targetTotalTxns);

  applyWarmStartDaysSincePayday(*state_, prepared_->population().spenders);

  diag::spending::Stats::instance().reset();
  preparedOnce_ = true;
}

std::uint32_t Session::dayIndexOf(time::TimePoint tp) const {
  const auto startSec = time::toEpochSeconds(market_.bounds().startDate);
  const auto tpSec = time::toEpochSeconds(tp);

  if (tpSec < startSec || (tpSec - startSec) % time::kSecondsPerDay != 0) {
    throw std::invalid_argument(
        "Session: window start is not day-aligned with market bounds");
  }

  return static_cast<std::uint32_t>((tpSec - startSec) / time::kSecondsPerDay);
}

void Session::collectNewRows(std::vector<transactions::Transaction> rows) {
  if (cards_ != nullptr && cards_->active()) {
    auto cardRows = cards_->takeEmitted();
    cardEventCount_ += cardRows.size();
    rows.reserve(rows.size() + cardRows.size());
    rows.insert(rows.end(), std::make_move_iterator(cardRows.begin()),
                std::make_move_iterator(cardRows.end()));
  }

  if (rows.empty()) {
    return;
  }

  sortChronological(rows);

  const auto finalizedEpoch = time::toEpochSeconds(finalizedThrough_);
  if (rows.front().timestamp < finalizedEpoch) {
    throw std::logic_error(
        "Session observed a late-generated transaction before its finalized "
        "watermark; increase or redesign the card finalization lag");
  }

  if (pendingTxns_.empty()) {
    pendingTxns_ = std::move(rows);
    return;
  }

  std::vector<transactions::Transaction> merged;
  merged.reserve(pendingTxns_.size() + rows.size());
  std::merge(std::make_move_iterator(pendingTxns_.begin()),
             std::make_move_iterator(pendingTxns_.end()),
             std::make_move_iterator(rows.begin()),
             std::make_move_iterator(rows.end()), std::back_inserter(merged),
             chronologicalComparator());
  pendingTxns_ = std::move(merged);
}

WindowOutput Session::drainFinalized(time::Window advancedWindow,
                                     time::TimePoint finalizedBoundExcl) {
  const auto marketEnd = time::addDays(market_.bounds().startDate,
                                       static_cast<int>(market_.bounds().days));

  finalizedBoundExcl =
      std::clamp(finalizedBoundExcl, finalizedThrough_, marketEnd);

  const auto boundEpoch = time::toEpochSeconds(finalizedBoundExcl);

  const auto split = std::lower_bound(
      pendingTxns_.begin(), pendingTxns_.end(), boundEpoch,
      [](const transactions::Transaction &txn, std::int64_t bound) {
        return txn.timestamp < bound;
      });

  std::vector<transactions::Transaction> out;
  out.reserve(static_cast<std::size_t>(split - pendingTxns_.begin()));

  out.insert(out.end(), std::make_move_iterator(pendingTxns_.begin()),
             std::make_move_iterator(split));

  pendingTxns_.erase(pendingTxns_.begin(), split);

  const time::Window finalizedWindow{
      .start = finalizedThrough_,
      .days = dayDistance(finalizedThrough_, finalizedBoundExcl),
  };

  finalizedThrough_ = finalizedBoundExcl;

  PL_LOG_DEBUG(sim,
               "Session advanced %d days; finalized %d days; rows=%zu; "
               "pending=%zu",
               advancedWindow.days, finalizedWindow.days, out.size(),
               pendingTxns_.size());

  return WindowOutput{
      .advancedWindow = advancedWindow,
      .finalizedWindow = finalizedWindow,
      .txns = std::move(out),
  };
}

WindowOutput Session::advance(time::Window window) {
  if (finished_) {
    throw std::logic_error("Session::advance called after finish()");
  }

  if (window.days < 0) {
    throw std::invalid_argument("Session::advance requires window.days >= 0");
  }

  if (market_.bounds().days == 0) {
    if (window.days != 0 || window.start != market_.bounds().startDate ||
        nextDayIndex_ != 0) {
      throw std::logic_error(
          "Session::advance window does not match the zero-day market");
    }

    return WindowOutput{
        .advancedWindow = window,
        .finalizedWindow = window,
        .txns = {},
    };
  }

  if (window.days == 0) {
    throw std::invalid_argument(
        "Session::advance requires a positive window for a non-empty market");
  }

  prepareOnce();

  const auto beginIdx = dayIndexOf(window.start);

  if (beginIdx != nextDayIndex_) {
    throw std::logic_error(
        "Session::advance windows must be contiguous: expected day " +
        std::to_string(nextDayIndex_) + ", got " + std::to_string(beginIdx));
  }

  const auto endIdx = beginIdx + static_cast<std::uint32_t>(window.days);

  if (endIdx > market_.bounds().days) {
    throw std::invalid_argument(
        "Session::advance window exceeds market bounds");
  }

  state_->txns().reserve(
      state_->txns().size() +
      static_cast<std::size_t>(reservePerDay_ *
                               static_cast<double>(window.days)));

  for (std::uint32_t dayIndex = beginIdx; dayIndex < endIdx; ++dayIndex) {
    dayDriver_.runDay(*prepared_, *state_, dayIndex);
    diag::spending::Stats::instance().recordDaySnapshot(dayIndex);
  }

  nextDayIndex_ = endIdx;

  collectNewRows(dayDriver_.takeEmitted(*state_));

  const auto candidateBound =
      time::addDays(window.endExcl(), -kCardFinalizationLagDays);

  const auto safeBound = std::max(candidateBound, finalizedThrough_);

  return drainFinalized(window, safeBound);
}

WindowOutput Session::finish() {
  if (finished_) {
    throw std::logic_error("Session::finish called twice");
  }

  const auto marketEnd = time::addDays(market_.bounds().startDate,
                                       static_cast<int>(market_.bounds().days));

  if (market_.bounds().days == 0) {
    finished_ = true;

    return WindowOutput{
        .advancedWindow =
            time::Window{.start = market_.bounds().startDate, .days = 0},
        .finalizedWindow =
            time::Window{.start = market_.bounds().startDate, .days = 0},
        .txns = {},
    };
  }

  if (nextDayIndex_ != market_.bounds().days) {
    throw std::logic_error(
        "Session::finish requires the full market range to be advanced");
  }

  prepareOnce();

  dayDriver_.finish(*state_);
  collectNewRows(dayDriver_.takeEmitted(*state_));

  auto cardResidual = dayDriver_.drainCardCycles();
  cardEventCount_ += cardResidual.size();
  collectNewRows(std::move(cardResidual));

  diag::spending::Stats::instance().dump();
  finished_ = true;

  return drainFinalized(time::Window{.start = marketEnd, .days = 0}, marketEnd);
}

} // namespace PhantomLedger::activity::spending::simulator
