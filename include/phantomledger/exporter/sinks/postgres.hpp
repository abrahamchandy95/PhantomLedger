#pragma once

#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/pipeline/chunk/schedule.hpp"
#include "phantomledger/pipeline/chunk/sink.hpp"
#include "phantomledger/primitives/io/callback_streambuf.hpp"
#include "phantomledger/primitives/postgres/connection.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstdint>
#include <optional>
#include <ostream>
#include <span>
#include <string>

namespace PhantomLedger::exporter::sinks {

// Streams settled spans into PostgreSQL, one COPY per span. Every row
// carries two bookkeeping columns that make stream identity a DEFINED
// property of the table:
//
//   row_seq     1-based global insertion ordinal — `ORDER BY row_seq`
//               reconstructs the exact stream (the read-back contract
//               that derived-analytics passes and digest verification
//               depend on; heap order is guaranteed by nothing)
//   span_index  the settlement span that committed the row
//
// Restart semantics (pinned by test_postgres): constructing a sink with
// createTable (the default) drops and recreates the table — a rerun is a
// full rewrite. An abandoned mid-span COPY commits nothing; previously
// committed spans survive a crash intact.
//
// Resume mode (RunLedger + ResumableSpanSink, pinned by test_resume):
// createTable=false, truncateFirst=false, startRowSeq=<committed rows>
// keeps the durable prefix and continues row_seq where the interrupted
// run stopped; the committed spans' COPYs are gated off upstream.
// rowsWritten() is therefore always the highest row_seq written — the
// total durable row count, not just this process's COPYs.
class Postgres {
public:
  struct Options {
    // libpq conninfo, e.g. "dbname=phantomledger host=localhost".
    std::string conninfo;
    std::string table = "transactions";
    bool createTable = true;
    bool truncateFirst = true;
    bool unlogged = true;

    // First row this sink writes gets row_seq startRowSeq + 1. Nonzero
    // only when resuming onto an existing durable prefix.
    std::uint64_t startRowSeq = 0;
  };

  explicit Postgres(Options options);

  void beginSpan(const pipeline::chunk::Span &span);
  void append(std::span<const transactions::Transaction> txns);
  void endSpan(const pipeline::chunk::Span &span);
  void finish();

  [[nodiscard]] std::uint64_t rowsWritten() const noexcept { return rows_; }
  [[nodiscard]] std::uint64_t spansWritten() const noexcept { return spans_; }

private:
  Options options_;
  postgres::Connection conn_;
  io::CallbackStreambuf streambuf_;
  std::ostream stream_;
  std::optional<csv::Writer> writer_;
  std::optional<postgres::CopyIn> copy_;

  std::uint64_t rows_ = 0;
  std::uint64_t spans_ = 0;
  unsigned spanIndex_ = 0;
  bool finished_ = false;
};

static_assert(pipeline::chunk::Sink<Postgres>);

} // namespace PhantomLedger::exporter::sinks
