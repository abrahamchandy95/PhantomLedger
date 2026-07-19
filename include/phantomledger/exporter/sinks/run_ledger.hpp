#pragma once
//
// phantomledger/exporter/sinks/run_ledger.hpp
//
// Run bookkeeping and checkpoint/resume for the PostgreSQL stream —
// determinism IS the checkpoint. Nothing here ever serializes generator
// state: an interrupted run is resumed by REGENERATING the stream from
// scratch (identical seed + config => identical bytes), verifying every
// already-durable span against its recorded content digest as the
// regenerated stream passes, skipping those spans' COPYs, and resuming
// writes at the first uncommitted span. What resume saves is COPY and
// durability cost, never generation time.
//
// Storage growth contract (owner requirement: rerunning many times must
// not accumulate data): every data table and mirror schema is a full
// DROP+CREATE rewrite per run, so at most one dataset per use case ever
// exists. pl_run_spans holds RESUME STATE ONLY — rows exist just for
// in-flight ('running') runs and are pruned the moment a run completes,
// fails, or is superseded. pl_run_manifest is the single append-only
// artifact: one small audit row per run, by design.
//
// Components (each independently testable — test_resume drives them
// without the CLI):
//
//   RunLedger          owns pl_run_manifest / pl_run_spans: config
//                      hashes, run status (running/complete/superseded/
//                      failed; pre-migration rows read 'legacy'),
//                      per-span row counts + BLAKE2b digests, resumable-
//                      run lookup, tail trim and structural verification
//   LedgerSpanHasher   per-span BLAKE2b over the SAME canonical row
//                      rendering the golden hashes
//                      (common::writeLedgerRows) — one rendering
//                      definition, two consumers
//   GatedSink          forwards to an inner sink only while open; the
//                      valve that lets golden + CSV exporters see every
//                      regenerated row while already-durable spans skip
//                      the COPY leg
//   ResumableSpanSink  the outermost span sink: records (span, rows,
//                      digest) rows for fresh spans; for committed spans
//                      it verifies rows/digest against the ledger AND
//                      runs a lockstep read-back scan comparing every
//                      durable row's decodable fields (bit-exact, per
//                      the pinned decode contract) against the
//                      regenerated stream
//
// Safety model, in order of the checks that enforce it:
//
//   1. A fresh run (any engine) marks every 'running' manifest row
//      superseded before rewriting the shared table, so a stale crash
//      record can never be resumed over another run's rows.
//   2. findResumable only returns a plan whose committed spans form a
//      contiguous prefix 0..k-1 with digests present.
//   3. prepareResume trims uncommitted tail rows (row_seq past the
//      committed count — e.g. a span whose COPY landed but whose ledger
//      row didn't) and structurally verifies the prefix: exact count,
//      max/distinct row_seq, and the per-span row profile. Any failure
//      means the table changed since the crash (including an UNLOGGED
//      table truncated by a server-side crash recovery): the run is
//      unresumable and the caller falls back to a clean full rewrite.
//   4. During the fold, every committed span must reproduce its recorded
//      row count and digest from the regenerated stream (catches config
//      drift the hash missed and any code change that altered output),
//      and every durable row must match the regenerated row bit-for-bit
//      on the decodable fields. Mismatch here is a hard error, never a
//      silent rewrite: generation is already underway and determinism
//      itself is in question.
//

#include "phantomledger/encoding/render.hpp"
#include "phantomledger/exporter/common/ledger.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/exporter/sinks/txn_readback.hpp"
#include "phantomledger/pipeline/chunk/schedule.hpp"
#include "phantomledger/pipeline/chunk/sink.hpp"
#include "phantomledger/primitives/crypto/blake2b.hpp"
#include "phantomledger/primitives/io/callback_streambuf.hpp"
#include "phantomledger/primitives/postgres/connection.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/transactions/record.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace PhantomLedger::exporter::sinks {

// One span of an interrupted run that is already durable in the table.
struct CommittedSpan {
  std::uint32_t index = 0;
  std::uint64_t rows = 0;
  std::string digest; // hex BLAKE2b of the span's canonical rendering
};

struct ResumePlan {
  long long manifestId = -1;
  std::vector<CommittedSpan> spans; // ascending, contiguous from 0
  std::uint64_t rows = 0;           // total rows across committed spans
};

class RunLedger {
public:
  struct Tables {
    std::string manifest = "pl_run_manifest";
    std::string spans = "pl_run_spans";
  };

  // Two constructors instead of a defaulted Tables argument: a default
  // member initializer of a nested class cannot be used in a default
  // argument while the enclosing class is still incomplete (Clang
  // rejects it), so the no-Tables form delegates from the .cpp where
  // Tables is complete.
  explicit RunLedger(postgres::Connection &conn);
  RunLedger(postgres::Connection &conn, Tables tables);

  // Idempotent: creates the tables when absent and migrates old shapes
  // in place (ADD COLUMN IF NOT EXISTS for config_hash/status/
  // span_digest; pre-migration manifest rows read status 'legacy' and
  // NULL config_hash, so they can never match a resume lookup).
  void ensureTables();

  // Canonical hash of everything that determines the stream bytes: a
  // format version, the engine, and seed/population/days/start. The
  // use case is deliberately EXCLUDED — the transactions table depends
  // only on the corpus, so a crashed standard run is resumable by an
  // aml run with the same config.
  [[nodiscard]] static std::string
  configHash(std::string_view engine, std::uint64_t seed,
             std::int32_t population, std::int64_t days,
             ::PhantomLedger::time::CalendarDate start);

  // Fresh runs rewrite the shared table: mark every 'running' manifest
  // row superseded first so no stale crash record outlives its rows,
  // and prune the span journal down to in-flight runs only (after the
  // update there are none, so this bounds pl_run_spans permanently).
  void supersedeRunning();

  [[nodiscard]] long long beginRun(const std::string &configHash,
                                   std::uint64_t seed,
                                   std::int32_t population, std::int64_t days,
                                   ::PhantomLedger::time::CalendarDate start);

  // Latest 'running' manifest with this config hash, or nullopt. Only
  // returns plans that are structurally resumable: at least one span,
  // contiguous indexes from 0, a digest recorded for every span.
  [[nodiscard]] std::optional<ResumePlan>
  findResumable(const std::string &configHash);

  // Trims the uncommitted tail (row_seq past the committed count) and
  // structurally verifies the durable prefix of `txnTable`. Returns
  // false — never throws for data mismatches — when the table no longer
  // matches the plan (modified out of band, truncated by server crash
  // recovery of an UNLOGGED table, dropped): the caller marks the run
  // failed and falls back to a clean full rewrite.
  [[nodiscard]] bool prepareResume(const ResumePlan &plan,
                                   const std::string &txnTable);

  // digest may be empty (reference-engine runs record counts only and
  // are never resumable).
  void recordSpan(long long manifestId, std::uint32_t spanIndex,
                  std::uint64_t rows, const std::string &digest);

  // Marks the run complete and prunes its span rows: resume state is
  // only meaningful for in-flight runs.
  void finishRun(long long manifestId, std::uint64_t rows,
                 const std::string &digest);

  // Marks the run failed and prunes its span rows.
  void markFailed(long long manifestId);

private:
  void pruneSpans(long long manifestId);

  postgres::Connection *conn_;
  Tables tables_;
};

namespace detail {

[[nodiscard]] inline std::string
hexDigest(std::span<const std::uint8_t> bytes) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.reserve(bytes.size() * 2);
  for (const auto b : bytes) {
    out.push_back(kHex[b >> 4U]);
    out.push_back(kHex[b & 0x0FU]);
  }
  return out;
}

} // namespace detail

// Per-span BLAKE2b over the canonical ledger rendering — the exact
// bytes sinks::Golden hashes (common::writeLedgerRows), so a span
// digest is a content statement about the stream, not about any sink.
class LedgerSpanHasher {
public:
  static constexpr std::size_t kDigestBytes = 32;

  LedgerSpanHasher()
      : streambuf_([this](const char *data, std::size_t size) {
          if (!hasher_.has_value() || !hasher_->update(data, size)) {
            throw std::logic_error("LedgerSpanHasher: bytes outside a span");
          }
        }),
        stream_(&streambuf_) {
    writer_.emplace(stream_);
  }

  void begin() { hasher_.emplace(kDigestBytes); }

  void update(std::span<const transactions::Transaction> txns) {
    common::writeLedgerRows(*writer_, txns);
  }

  [[nodiscard]] std::string finish() {
    stream_.flush();
    std::array<std::uint8_t, kDigestBytes> bytes{};
    if (!hasher_.has_value() || !hasher_->finalize(bytes.data(), bytes.size())) {
      throw std::logic_error("LedgerSpanHasher: finalize failed");
    }
    hasher_.reset();
    return detail::hexDigest(bytes);
  }

private:
  std::optional<crypto::blake2b::Stream> hasher_;
  io::CallbackStreambuf streambuf_;
  std::ostream stream_;
  std::optional<csv::Writer> writer_;
};

// Forwards to the inner sink only while open. finish() always forwards
// (lifecycle, not data). The COPY leg sits behind one of these so
// already-durable spans skip PostgreSQL while golden and the streaming
// CSV exporters still see every regenerated row.
template <pipeline::chunk::Sink Inner> struct GatedSink {
  Inner *inner = nullptr;
  bool open = true;

  void beginSpan(const pipeline::chunk::Span &span) {
    if (open) {
      inner->beginSpan(span);
    }
  }

  void append(std::span<const transactions::Transaction> txns) {
    if (open) {
      inner->append(txns);
    }
  }

  void endSpan(const pipeline::chunk::Span &span) {
    if (open) {
      inner->endSpan(span);
    }
  }

  void finish() { inner->finish(); }

  [[nodiscard]] std::uint64_t rowsWritten() const {
    return inner->rowsWritten();
  }
};

// The outermost span sink for PostgreSQL-connected runs — the windowed
// twin of the monolithic flushPartitioned callback, extended with
// checkpoint/resume:
//
//   fresh span      forward everything, then record
//                   (span, rows, digest) in pl_run_spans
//   committed span  close the COPY gate, verify the regenerated span
//                   reproduces the recorded rows + digest, and compare
//                   every durable row (lockstep TransactionScan, pinned
//                   lossless decode) against the regenerated row
//
// Committed spans are always a stream prefix (spans commit in order),
// so the lockstep scan reads the table once, front to back, and is
// released the moment the prefix ends.
template <pipeline::chunk::Sink Inner> class ResumableSpanSink {
public:
  struct Config {
    Inner *inner = nullptr;
    bool *copyGate = nullptr; // GatedSink::open of the COPY leg
    RunLedger *ledger = nullptr;
    long long manifestId = -1;
    const ResumePlan *plan = nullptr; // nullptr => fresh run
    std::string conninfo;             // lockstep scan (resume only)
    std::string txnTable = "transactions";
  };

  explicit ResumableSpanSink(Config cfg) : cfg_(std::move(cfg)) {
    if (cfg_.plan != nullptr && !cfg_.plan->spans.empty()) {
      scanConn_.emplace(cfg_.conninfo);
      scan_.emplace(*scanConn_, cfg_.txnTable);
    }
  }

  void beginSpan(const pipeline::chunk::Span &span) {
    skipping_ = inPrefix();
    if (skipping_ && span.index != cfg_.plan->spans[cursor_].index) {
      throw std::runtime_error(
          "resume: regenerated span order diverges from the committed "
          "prefix (got span " +
          std::to_string(span.index) + ", ledger has span " +
          std::to_string(cfg_.plan->spans[cursor_].index) +
          ") — the span schedule changed since the interrupted run");
    }
    if (cfg_.copyGate != nullptr) {
      *cfg_.copyGate = !skipping_;
    }
    hasher_.begin();
    spanRows_ = 0;
    cfg_.inner->beginSpan(span);
  }

  void append(std::span<const transactions::Transaction> txns) {
    hasher_.update(txns);
    if (skipping_) {
      verifyDurable(txns);
    }
    spanRows_ += txns.size();
    cfg_.inner->append(txns);
  }

  void endSpan(const pipeline::chunk::Span &span) {
    cfg_.inner->endSpan(span);
    const auto digest = hasher_.finish();
    if (skipping_) {
      const auto &committed = cfg_.plan->spans[cursor_];
      if (spanRows_ != committed.rows) {
        throw std::runtime_error(
            "resume: span " + std::to_string(span.index) + " regenerated " +
            std::to_string(spanRows_) + " rows but the ledger committed " +
            std::to_string(committed.rows) +
            " — output changed since the interrupted run");
      }
      if (digest != committed.digest) {
        throw std::runtime_error(
            "resume: span " + std::to_string(span.index) +
            " digest mismatch — the regenerated stream is not the one the "
            "interrupted run wrote (config drift or a code change); rerun "
            "to rewrite in full");
      }
      ++cursor_;
      ++skipped_;
      if (!inPrefix()) {
        // Prefix fully verified: release the read transaction.
        scan_.reset();
        scanConn_.reset();
      }
    } else {
      cfg_.ledger->recordSpan(cfg_.manifestId, span.index, spanRows_, digest);
      ++written_;
    }
  }

  void finish() {
    if (inPrefix()) {
      throw std::runtime_error(
          "resume: stream ended inside the committed prefix — the "
          "regenerated stream is shorter than the interrupted run's");
    }
    cfg_.inner->finish();
  }

  [[nodiscard]] std::uint64_t rowsWritten() const {
    return cfg_.inner->rowsWritten();
  }

  [[nodiscard]] unsigned spansWritten() const noexcept { return written_; }
  [[nodiscard]] unsigned spansSkipped() const noexcept { return skipped_; }

private:
  [[nodiscard]] bool inPrefix() const noexcept {
    return cfg_.plan != nullptr && cursor_ < cfg_.plan->spans.size();
  }

  void verifyDurable(std::span<const transactions::Transaction> txns) {
    postgres::StreamTxnRow row;
    for (const auto &tx : txns) {
      ++durableRow_;
      if (!scan_->next(row) || row.rowSeq != durableRow_) {
        throw std::runtime_error(
            "resume: durable row " + std::to_string(durableRow_) +
            " missing from the transactions table — the table changed "
            "since the interrupted run");
      }
      const bool same =
          row.timestamp == tx.timestamp && row.fraudFlag == tx.fraud.flag &&
          std::bit_cast<std::uint64_t>(row.amount) ==
              std::bit_cast<std::uint64_t>(tx.amount) &&
          row.sourceRendered ==
              std::string_view{encoding::format(tx.source).view()} &&
          row.targetRendered ==
              std::string_view{encoding::format(tx.target).view()};
      if (!same) {
        throw std::runtime_error(
            "resume: durable row " + std::to_string(durableRow_) +
            " does not match the regenerated stream — the table changed "
            "since the interrupted run");
      }
    }
  }

  Config cfg_;
  LedgerSpanHasher hasher_;

  std::optional<postgres::Connection> scanConn_;
  std::optional<postgres::TransactionScan> scan_;

  std::size_t cursor_ = 0;
  std::uint64_t spanRows_ = 0;
  std::uint64_t durableRow_ = 0;
  unsigned written_ = 0;
  unsigned skipped_ = 0;
  bool skipping_ = false;
};

static_assert(pipeline::chunk::Sink<GatedSink<pipeline::chunk::NullSink>>);
static_assert(
    pipeline::chunk::Sink<ResumableSpanSink<pipeline::chunk::NullSink>>);

} // namespace PhantomLedger::exporter::sinks
