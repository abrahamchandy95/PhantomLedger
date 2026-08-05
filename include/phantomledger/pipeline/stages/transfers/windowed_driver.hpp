#pragma once
//
// phantomledger/pipeline/stages/transfers/windowed_driver.hpp
//
// Chronological transfer fold over bounded generation windows.
//
// SETTLEMENT SEMANTICS
// The fold reuses ChronoReplayAccumulator::extendChunk() with the same
// active/lookahead slicing rules as LedgerReplay::preFraudChunked() and
// LedgerReplay::postFraudChunkedMerged(). A settlement span is settled only
// once finalized generation coverage reaches span.lookaheadBoundExcl, so
// insufficient-funds retries, future-inbound cure searches, and drop
// decisions observe the same lookahead rows as the authoritative full-range
// chunked replay. The previous approach — extend() followed by a delayed
// output watermark — settled every row immediately against a window-local
// cure index and is NOT equivalent; it has been removed.
//
// The total simulation window remains half-open. In the final span,
// generated rows at/after the run end may still be indexed as lookahead for
// an active row's cure decision, but they are never emitted, counted in the
// fraud denominator, or applied to either ledger book.
//
// FRAUD BOUNDARY
// The fraud budget denominator is the exact realized pre-fraud candidate
// count. Phase A therefore runs to completion (generation + pre-fraud fold
// + candidate emission) before any fraud is generated:
//
//   Phase A:  session/sources -> pre-fraud extendChunk fold -> candidate sink
//   Boundary: exact realized count L -> caller builds the fraud source
//   Phase B:  candidate cursor + fraud cursor -> post-fraud fold -> sink
//
// runTwoPhase() wires this in-process with an in-memory candidate spool.
// The spool is a correctness rung only: its interface (a chunk sink on the
// way in, a ScheduleCursorSource on the way out) is designed to be replaced
// by a binary spool file or a PostgreSQL staging table without changing
// fold semantics.
//
// RNG LANES
// Generation, pre-fraud settlement and post-fraud settlement must not share
// one sequential RNG stream. Window boundaries alter when settlement work
// occurs, so shared RNG consumption would make output depend on window
// size. This driver therefore owns two deterministic settlement lanes:
//
//   settlement / pre_fraud
//   settlement / post_fraud
//
// The accumulators retain pointers to these owned RNGs for the driver's
// complete lifetime.
//
// SCHEDULE
// config.settlement drives BOTH folds, exactly as the monolithic path uses
// one replaySchedule for preFraudChunked and postFraudChunkedMerged. The
// per-phase lookahead-day fields of the previous WindowedConfig have been
// removed; settlement.lookaheadDays is the single source of truth.
//

#include "phantomledger/activity/spending/simulator/session.hpp"
#include "phantomledger/pipeline/chunk/schedule.hpp"
#include "phantomledger/pipeline/transfers.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/ledger/posting.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace PhantomLedger::pipeline::stages::transfers {

// ---------------------------------------------------------------- sources

class WindowTxnSource {
public:
  virtual ~WindowTxnSource() = default;

  [[nodiscard]] virtual std::vector<transactions::Transaction>
  emit(time::Window window) = 0;
};

class ScheduleCursorSource {
public:
  virtual ~ScheduleCursorSource() = default;

  virtual void emitUntil(std::int64_t endExclusiveEpochSeconds,
                         std::vector<transactions::Transaction> &out) = 0;
};

// ---------------------------------------------------------- sink erasure

class SinkRef {
public:
  template <class S>
  explicit SinkRef(S &sink)
      : self_(&sink), begin_([](void *pointer, const chunk::Span &span) {
          static_cast<S *>(pointer)->beginSpan(span);
        }),
        append_(
            [](void *pointer, std::span<const transactions::Transaction> txns) {
              static_cast<S *>(pointer)->append(txns);
            }),
        end_([](void *pointer, const chunk::Span &span) {
          static_cast<S *>(pointer)->endSpan(span);
        }),
        finish_([](void *pointer) { static_cast<S *>(pointer)->finish(); }),
        rows_([](const void *pointer) -> std::uint64_t {
          return static_cast<std::uint64_t>(
              static_cast<const S *>(pointer)->rowsWritten());
        }) {}

  void beginSpan(const chunk::Span &span) { begin_(self_, span); }

  void append(std::span<const transactions::Transaction> txns) {
    append_(self_, txns);
  }

  void endSpan(const chunk::Span &span) { end_(self_, span); }

  void finish() { finish_(self_); }

  [[nodiscard]] std::uint64_t rowsWritten() const { return rows_(self_); }

private:
  void *self_ = nullptr;

  void (*begin_)(void *, const chunk::Span &) = nullptr;

  void (*append_)(void *, std::span<const transactions::Transaction>) = nullptr;

  void (*end_)(void *, const chunk::Span &) = nullptr;

  void (*finish_)(void *) = nullptr;

  std::uint64_t (*rows_)(const void *) = nullptr;
};

// ------------------------------------------------------- candidate spool

// In-memory candidate spool. Correctness rung only: retains the complete
// accepted candidate corpus for the run. Its memory cost is O(L) rows and
// is documented honestly; a durable spool replaces it in a later phase.
class VectorCandidateSpool {
public:
  void beginSpan(const chunk::Span &) noexcept {}

  void append(std::span<const transactions::Transaction> txns) {
    rows_.insert(rows_.end(), txns.begin(), txns.end());
  }

  void endSpan(const chunk::Span &) noexcept {}

  void finish() noexcept {}

  [[nodiscard]] std::uint64_t rowsWritten() const noexcept {
    return static_cast<std::uint64_t>(rows_.size());
  }

  // Rows arrive in settlement-span order and each span batch is
  // replay-sorted, so the whole vector is replay-sorted.
  [[nodiscard]] std::vector<transactions::Transaction> takeRows() noexcept {
    return std::move(rows_);
  }

private:
  std::vector<transactions::Transaction> rows_;
};

// ---------------------------------------------------------------- driver

struct WindowedConfig {
  chunk::Strategy generation{
      .monthsPerChunk = 3,
      .lookaheadDays = 35,
  };

  chunk::Strategy settlement{
      .monthsPerChunk = 1,
      .lookaheadDays = 35,
  };

  ::PhantomLedger::transfers::legit::ledger::ReplayFundingBehavior funding{};

  // Matches LedgerReplay::preFraud().
  bool preEmitLiquidityEvents = true;

  // Matches LedgerReplay::postFraud().
  bool postEmitLiquidityEvents = false;
};

struct PhaseAResult {
  // Exact number of accepted pre-fraud candidate rows (L). This is the
  // fraud-budget denominator. It includes accepted product rows and
  // liquidity-event rows within the half-open run window, and excludes
  // unrecoverable drops and final-lookahead rows.
  std::uint64_t candidateRows = 0;

  std::uint64_t legitRows = 0;
  std::uint64_t sourceRows = 0;
  std::uint64_t cardEvents = 0;

  ReplayDrops preDrops{};
};

struct PhaseBResult {
  std::uint64_t rowsFlushed = 0;
  std::uint64_t candidateRows = 0;
  std::uint64_t fraudRows = 0;

  ReplayDrops postDrops{};
};

struct RunSummary {
  PhaseAResult phaseA{};
  PhaseBResult phaseB{};
};

class WindowedTransferDriver {
public:
  using Session = ::PhantomLedger::activity::spending::simulator::Session;

  using Accumulator =
      ::PhantomLedger::transfers::legit::ledger::ChronoReplayAccumulator;

  // Builds the fraud cursor source at the Phase A / Phase B boundary. The
  // argument is the exact realized candidate count L. Returning nullptr
  // runs Phase B without fraud.
  using FraudSourceFactory =
      std::function<std::unique_ptr<ScheduleCursorSource>(
          std::uint64_t realizedCandidateCount)>;

  WindowedTransferDriver(std::unique_ptr<clearing::Ledger> preBook,
                         std::unique_ptr<clearing::Ledger> postBook,
                         const random::RngFactory &rngFactory,
                         WindowedConfig config);

  WindowedTransferDriver &session(Session &value) noexcept;

  WindowedTransferDriver &addCalendarSource(WindowTxnSource &value);

  WindowedTransferDriver &addCursorSource(ScheduleCursorSource &value);

  // Phase A: generation + pre-fraud fold. The sink receives the accepted
  // candidate rows partitioned by settlement span. Single use.
  template <class S>
  [[nodiscard]] PhaseAResult runPhaseA(time::Window full, S &sink) {
    SinkRef ref(sink);
    return runPhaseAErased(full, ref);
  }

  // Phase B: candidate cursor + optional fraud cursor through the
  // post-fraud fold. Both cursors must be replay-sorted. Single use.
  template <class S>
  [[nodiscard]] PhaseBResult runPhaseB(time::Window full,
                                       ScheduleCursorSource &candidates,
                                       ScheduleCursorSource *fraud, S &sink) {
    SinkRef ref(sink);
    return runPhaseBErased(full, candidates, fraud, ref);
  }

  // Both phases in-process with an in-memory candidate spool. makeFraud is
  // invoked exactly once, after Phase A, with the exact realized count.
  template <class S>
  [[nodiscard]] RunSummary
  runTwoPhase(time::Window full, const FraudSourceFactory &makeFraud, S &sink) {
    SinkRef ref(sink);
    return runTwoPhaseErased(full, makeFraud, ref);
  }

  [[nodiscard]] const clearing::Ledger &postedBook() const noexcept {
    return *postBook_;
  }

  // Hands off ownership of the final posted book (the AML exporters'
  // account vertices need its balances). Call only AFTER Phase B has
  // completed: the post accumulator retains a pointer into this book, so
  // the driver must not fold again after the handoff.
  [[nodiscard]] std::unique_ptr<clearing::Ledger> takePostedBook() noexcept {
    return std::move(postBook_);
  }

private:
  [[nodiscard]] PhaseAResult runPhaseAErased(time::Window full, SinkRef sink);

  [[nodiscard]] PhaseBResult runPhaseBErased(time::Window full,
                                             ScheduleCursorSource &candidates,
                                             ScheduleCursorSource *fraud,
                                             SinkRef sink);

  [[nodiscard]] RunSummary
  runTwoPhaseErased(time::Window full, const FraudSourceFactory &makeFraud,
                    SinkRef sink);

  void stagePreRows(std::vector<transactions::Transaction> legit,
                    time::Window window, PhaseAResult &summary);

  void settlePreSpans(bool finished, SinkRef &sink, PhaseAResult &summary);

  std::unique_ptr<clearing::Ledger> preBook_;
  std::unique_ptr<clearing::Ledger> postBook_;

  // These must be declared before preAcc_ and postAcc_. The accumulators
  // retain pointers to the RNGs.
  random::Rng preRng_;
  random::Rng postRng_;

  WindowedConfig config_;

  Session *session_ = nullptr;

  std::vector<WindowTxnSource *> calendarSources_;
  std::vector<ScheduleCursorSource *> cursorSources_;

  std::unique_ptr<Accumulator> preAcc_;
  std::unique_ptr<Accumulator> postAcc_;

  // Replay-sorted rows generated but not yet fed to the pre-fraud fold as
  // an active slice. Bounded by generation window + card finalization lag
  // + settlement lookahead; consumed prefixes are erased per span.
  std::vector<transactions::Transaction> preStage_;
  std::int64_t preCoverageExcl_ = 0;

  std::vector<chunk::Span> settleSpans_;
  std::size_t nextPreSpan_ = 0;

  bool phaseARun_ = false;
  bool phaseBRun_ = false;
};

} // namespace PhantomLedger::pipeline::stages::transfers
