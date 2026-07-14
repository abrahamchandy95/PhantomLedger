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

class Postgres {
public:
  struct Options {
    // libpq conninfo, e.g. "dbname=phantomledger host=localhost".
    std::string conninfo;
    std::string table = "transactions";
    bool createTable = true;
    bool truncateFirst = true;
    bool unlogged = true;
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
  bool finished_ = false;
};

static_assert(pipeline::chunk::Sink<Postgres>);

} // namespace PhantomLedger::exporter::sinks
