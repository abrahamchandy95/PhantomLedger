#include "phantomledger/pipeline/stages/transfers/windowed_driver.hpp"

#include "phantomledger/diagnostics/logger.hpp"
#include "phantomledger/pipeline/diagnostics.hpp"
#include "phantomledger/pipeline/stages/transfers/window_sources.hpp"
#include "phantomledger/transfers/legit/ledger/streams.hpp"

#include <algorithm>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>

namespace PhantomLedger::pipeline::stages::transfers {

namespace {

namespace legit_ledger = ::PhantomLedger::transfers::legit::ledger;

using Txn = ::PhantomLedger::transactions::Transaction;

constexpr std::int64_t kSecondsPerDay = 86'400;

constexpr std::int64_t kMaxEpoch = std::numeric_limits<std::int64_t>::max();

[[nodiscard]] std::int64_t epochOf(time::TimePoint timePoint) noexcept {
  return time::toEpochSeconds(timePoint);
}

[[nodiscard]] std::int64_t windowEndEpoch(time::Window window) noexcept {
  return epochOf(window.start) +
         static_cast<std::int64_t>(window.days) * kSecondsPerDay;
}

// First index in `rows` at or after `boundExcl`, scanning from `from`.
// `rows` must be replay-sorted.
[[nodiscard]] std::size_t sliceTo(const std::vector<Txn> &rows,
                                  std::size_t from,
                                  std::int64_t boundExcl) noexcept {
  const auto tsBefore = [](const Txn &t, std::int64_t s) noexcept {
    return t.timestamp < s;
  };
  return static_cast<std::size_t>(
      std::lower_bound(rows.begin() + static_cast<std::ptrdiff_t>(from),
                       rows.end(), boundExcl, tsBefore) -
      rows.begin());
}

[[nodiscard]] std::vector<Txn> mergeSorted(std::vector<Txn> lhs,
                                           std::vector<Txn> rhs) {
  if (lhs.empty()) {
    return rhs;
  }

  if (rhs.empty()) {
    return lhs;
  }

  std::vector<Txn> merged;
  merged.reserve(lhs.size() + rhs.size());

  std::merge(
      std::make_move_iterator(lhs.begin()), std::make_move_iterator(lhs.end()),
      std::make_move_iterator(rhs.begin()), std::make_move_iterator(rhs.end()),
      std::back_inserter(merged), legit_ledger::detail::fundsLess);

  return merged;
}

} // namespace

WindowedTransferDriver::WindowedTransferDriver(
    std::unique_ptr<clearing::Ledger> preBook,
    std::unique_ptr<clearing::Ledger> postBook,
    const random::RngFactory &rngFactory, WindowedConfig config)
    : preBook_(std::move(preBook)), postBook_(std::move(postBook)),
      preRng_(rngFactory.rng({"settlement", "pre_fraud"})),
      postRng_(rngFactory.rng({"settlement", "post_fraud"})),
      config_(std::move(config)) {
  if (preBook_ == nullptr || postBook_ == nullptr) {
    throw std::invalid_argument(
        "WindowedTransferDriver requires two opening-book copies");
  }

  preAcc_ =
      std::make_unique<Accumulator>(preBook_.get(), &preRng_, config_.funding,
                                    config_.preEmitLiquidityEvents);

  postAcc_ =
      std::make_unique<Accumulator>(postBook_.get(), &postRng_, config_.funding,
                                    config_.postEmitLiquidityEvents);
}

WindowedTransferDriver &
WindowedTransferDriver::session(Session &value) noexcept {
  session_ = &value;
  return *this;
}

WindowedTransferDriver &
WindowedTransferDriver::addCalendarSource(WindowTxnSource &value) {
  calendarSources_.push_back(&value);
  return *this;
}

WindowedTransferDriver &
WindowedTransferDriver::addCursorSource(ScheduleCursorSource &value) {
  cursorSources_.push_back(&value);
  return *this;
}

// --------------------------------------------------------------- Phase A

PhaseAResult WindowedTransferDriver::runPhaseAErased(time::Window full,
                                                     SinkRef sink) {
  if (session_ == nullptr) {
    throw std::logic_error("WindowedTransferDriver requires a bound Session");
  }
  if (phaseARun_) {
    throw std::logic_error("WindowedTransferDriver: Phase A already run");
  }
  phaseARun_ = true;

  const auto generationSchedule =
      chunk::Schedule::partition(full, config_.generation);

  const auto settlementSchedule =
      chunk::Schedule::partition(full, config_.settlement);

  settleSpans_.assign(settlementSchedule.begin(), settlementSchedule.end());
  nextPreSpan_ = 0;
  preStage_.clear();
  preCoverageExcl_ = epochOf(full.start);

  PhaseAResult summary;

  const auto feedFinalized = [&](activity::spending::simulator::Batch batch,
                                 bool finished) {
    summary.legitRows += batch.txns.size();

    if (batch.finalized.days > 0) {
      const auto finalized = batch.finalized;
      stagePreRows(std::move(batch.txns), finalized, summary);
      preCoverageExcl_ = windowEndEpoch(finalized);
    }

    settlePreSpans(finished, sink, summary);

    /* Fold-residency probe: one line per generation span showing which
     * retention carries the growth. Phase A's buffers are all bounded --
     * preStage_ compacts per settled span, the pending queue drains per
     * chunk -- so the run peak accrues here. */
    pipeline::diagnostics::logStageMem(
        "phaseA:gen", {{"preStage", preStage_.size()},
                       {"prePending", preAcc_->pendingRows()},
                       {"preSettled", preAcc_->settledRows()}});
  };

  for (const auto &span : generationSchedule) {
    feedFinalized(session_->advance(span.activeWindow), /*finished=*/false);
  }

  feedFinalized(session_->finish(), /*finished=*/true);

  sink.finish();

  summary.cardEvents = session_->cardEventCount();

  summary.preDrops = ReplayDrops{
      .byReason = preAcc_->dropCounts(),
      .byChannel = preAcc_->dropCountsByChannel(),
  };

  PL_LOG_INFO(clearing,
              "phase A complete: candidates={} legit={} sources={} cards={}",
              summary.candidateRows, summary.legitRows, summary.sourceRows,
              summary.cardEvents);

  return summary;
}

void WindowedTransferDriver::stagePreRows(std::vector<Txn> legit,
                                          time::Window window,
                                          PhaseAResult &summary) {
  // Session rows are canonically sorted; canonical (full-audit) order is a
  // refinement of the funds-transfer replay key, so they are replay-sorted.
  auto rows = std::move(legit);

  for (auto *source : calendarSources_) {
    auto extra = legit_ledger::sortForReplay(source->emit(window));

    summary.sourceRows += extra.size();

    rows = mergeSorted(std::move(rows), std::move(extra));
  }

  const auto endExcl = windowEndEpoch(window);

  for (auto *source : cursorSources_) {
    // Cursor sources emit replay-sorted rows by contract
    // (PrecomputedCursorSource). Do not re-sort: std::sort is unstable and
    // could permute rows whose replay keys tie.
    std::vector<Txn> extra;

    source->emitUntil(endExcl, extra);

    summary.sourceRows += extra.size();

    rows = mergeSorted(std::move(rows), std::move(extra));
  }

  // Finalized windows are contiguous and ascending, and every cursor was
  // already drained through the previous window end, so this batch begins
  // at or after every retained timestamp. Appending preserves global
  // replay order.
  preStage_.insert(preStage_.end(), std::make_move_iterator(rows.begin()),
                   std::make_move_iterator(rows.end()));
}

void WindowedTransferDriver::settlePreSpans(bool finished, SinkRef &sink,
                                            PhaseAResult &summary) {
  while (nextPreSpan_ < settleSpans_.size()) {
    const auto &span = settleSpans_[nextPreSpan_];
    const bool last = (nextPreSpan_ + 1 == settleSpans_.size());

    // The final span still needs complete generated coverage: rows at or
    // beyond the run end remain useful for cure discovery, but the active
    // emit bound is always the half-open simulation-window boundary.
    // A non-final span needs generated coverage through its lookahead bound.
    if (last) {
      if (!finished) {
        break;
      }
    } else if (!finished &&
               epochOf(span.lookaheadBoundExcl) > preCoverageExcl_) {
      break;
    }

    const auto activeStart = epochOf(span.activeWindow.start);
    const auto bound = epochOf(span.activeWindow.endExcl());
    const auto begin = sliceTo(preStage_, 0, activeStart);
    const auto end = sliceTo(preStage_, begin, bound);
    const auto lookEnd =
        last ? preStage_.size()
             : sliceTo(preStage_, end, epochOf(span.lookaheadBoundExcl));

    preAcc_->extendChunk(
        std::span<const Txn>{preStage_.data() + begin, end - begin},
        std::span<const Txn>{preStage_.data() + end, lookEnd - end}, bound);

    auto settled = preAcc_->takeSettledBefore(bound);

    summary.candidateRows += settled.size();

    sink.beginSpan(span);
    sink.append(std::span<const Txn>(settled.data(), settled.size()));
    sink.endSpan(span);

    // Compact the consumed prefix so staging memory does not scale with
    // the simulation lifetime.
    preStage_.erase(preStage_.begin(),
                    preStage_.begin() + static_cast<std::ptrdiff_t>(end));

    ++nextPreSpan_;
  }
}

// --------------------------------------------------------------- Phase B

PhaseBResult WindowedTransferDriver::runPhaseBErased(
    time::Window full, ScheduleCursorSource &candidates,
    ScheduleCursorSource *fraud, SinkRef sink) {
  if (phaseBRun_) {
    throw std::logic_error("WindowedTransferDriver: Phase B already run");
  }
  phaseBRun_ = true;

  const auto settlementSchedule =
      chunk::Schedule::partition(full, config_.settlement);

  PhaseBResult summary;

  // Replay-sorted staging for both streams. Consumed prefixes are erased
  // per span, so slicing always starts at index zero.
  std::vector<Txn> candStage;
  std::vector<Txn> fraudStage;

  std::vector<Txn> slice;
  std::vector<Txn> look;

  const std::size_t spanCount = settlementSchedule.size();

  for (std::size_t k = 0; k < spanCount; ++k) {
    const auto &span = settlementSchedule[k];
    const bool last = (k + 1 == spanCount);

    const auto pullBound = last ? kMaxEpoch : epochOf(span.lookaheadBoundExcl);

    candidates.emitUntil(pullBound, candStage);

    if (fraud != nullptr) {
      fraud->emitUntil(pullBound, fraudStage);
    }

    const auto activeStart = epochOf(span.activeWindow.start);
    const auto bound = epochOf(span.activeWindow.endExcl());
    const auto cBegin = sliceTo(candStage, 0, activeStart);
    const auto fBegin = sliceTo(fraudStage, 0, activeStart);
    const auto cEnd = sliceTo(candStage, cBegin, bound);
    const auto fEnd = sliceTo(fraudStage, fBegin, bound);
    const auto lookSec = epochOf(span.lookaheadBoundExcl);
    const auto cLook =
        last ? candStage.size() : sliceTo(candStage, cEnd, lookSec);
    const auto fLook =
        last ? fraudStage.size() : sliceTo(fraudStage, fEnd, lookSec);

    // These are active-window source counts, not cursor-prefetch counts.
    // In particular, final-span fraud remediation rows may be retained as
    // lookahead but cannot inflate the fraud budget/reporting denominator.
    summary.candidateRows += cEnd - cBegin;
    summary.fraudRows += fEnd - fBegin;

    slice.clear();
    slice.reserve((cEnd - cBegin) + (fEnd - fBegin));
    std::merge(candStage.begin() + static_cast<std::ptrdiff_t>(cBegin),
               candStage.begin() + static_cast<std::ptrdiff_t>(cEnd),
               fraudStage.begin() + static_cast<std::ptrdiff_t>(fBegin),
               fraudStage.begin() + static_cast<std::ptrdiff_t>(fEnd),
               std::back_inserter(slice), legit_ledger::detail::fundsLess);

    // Cure discovery only indexes the lookahead; order is irrelevant.
    look.clear();
    look.reserve((cLook - cEnd) + (fLook - fEnd));
    look.insert(look.end(),
                candStage.begin() + static_cast<std::ptrdiff_t>(cEnd),
                candStage.begin() + static_cast<std::ptrdiff_t>(cLook));
    look.insert(look.end(),
                fraudStage.begin() + static_cast<std::ptrdiff_t>(fEnd),
                fraudStage.begin() + static_cast<std::ptrdiff_t>(fLook));

    postAcc_->extendChunk(std::span<const Txn>{slice.data(), slice.size()},
                          std::span<const Txn>{look.data(), look.size()},
                          bound);

    auto finalized = postAcc_->takeSettledBefore(bound);

    sink.beginSpan(span);
    sink.append(std::span<const Txn>(finalized.data(), finalized.size()));
    sink.endSpan(span);

    candStage.erase(candStage.begin(),
                    candStage.begin() + static_cast<std::ptrdiff_t>(cEnd));
    fraudStage.erase(fraudStage.begin(),
                     fraudStage.begin() + static_cast<std::ptrdiff_t>(fEnd));
  }

  sink.finish();

  summary.rowsFlushed = sink.rowsWritten();

  summary.postDrops = ReplayDrops{
      .byReason = postAcc_->dropCounts(),
      .byChannel = postAcc_->dropCountsByChannel(),
  };

  PL_LOG_INFO(clearing, "phase B complete: flushed={} candidates={} fraud={}",
              summary.rowsFlushed, summary.candidateRows, summary.fraudRows);

  return summary;
}

// ------------------------------------------------------------- two-phase

RunSummary WindowedTransferDriver::runTwoPhaseErased(
    time::Window full, const FraudSourceFactory &makeFraud, SinkRef sink) {
  RunSummary summary;

  VectorCandidateSpool spool;
  summary.phaseA = runPhaseA(full, spool);

  std::unique_ptr<ScheduleCursorSource> fraudSource;
  if (makeFraud) {
    fraudSource = makeFraud(summary.phaseA.candidateRows);
  }

  PrecomputedCursorSource candidates(spool.takeRows());

  summary.phaseB = runPhaseBErased(full, candidates, fraudSource.get(), sink);

  return summary;
}

} // namespace PhantomLedger::pipeline::stages::transfers
