#pragma once
//
// phantomledger/exporter/common/table.hpp
//
// Direct-table writing — the CSV retirement arc's end state. A
// common::Table renders each row ONCE through its csv::Writer into a
// single byte stream that feeds the table's PostgreSQL COPY (when the
// target's PgMirror is armed). PhantomLedger writes no files: the
// rendered bytes ARE the COPY payload, so what lands in PostgreSQL is
// the writer's output by construction — there is no second rendering
// to drift.
//
// With no mirror the rendering goes nowhere (a legal null sink: the
// serverless PL_FILE_ONLY harness runs this way and yields only the
// corpus stream digest).
//
// TableCapture is TEST INFRASTRUCTURE: a capture installed on the
// target receives every table's exact rendered bytes, keyed by the
// table's filename stem — the serverless seam test_pipeline_e2e uses
// to pin each exporter's table set now that there are no files to
// inspect. Production code never installs one.
//
// A Table converts implicitly to csv::Writer&, so every existing
// write function (writePartyRows, writeTransferRows, ...) works
// unchanged.
//

#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/exporter/schema.hpp"
#include "phantomledger/exporter/sinks/table_mirror.hpp"
#include "phantomledger/primitives/io/callback_streambuf.hpp"

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

namespace PhantomLedger::exporter::common {

// Test seam: receives the exact rendered bytes of every table opened
// against a target that carries it, keyed by the table's filename stem
// (e.g. "Party", "SAR"). Streamed tables deliver their bytes
// incrementally across the whole fold.
struct TableCapture {
  TableCapture() = default;
  TableCapture(const TableCapture &) = delete;
  TableCapture &operator=(const TableCapture &) = delete;
  TableCapture(TableCapture &&) = delete;
  TableCapture &operator=(TableCapture &&) = delete;
  virtual ~TableCapture() = default;

  virtual void put(std::string_view stem, const char *data,
                   std::size_t size) = 0;
};

// Where an exporter's tables go: a direct PostgreSQL table when `pg`
// is set, plus the test capture when one is installed. Exporters
// receive this from their Options/Config and never decide backend
// policy themselves.
struct TableTarget {
  const sinks::PgMirror *pg = nullptr; // nullptr => no direct table
  TableCapture *capture = nullptr;     // test infrastructure only
};

class Table {
public:
  Table(const TableTarget &target, const schema::Table &table)
      : guts_(std::make_unique<Guts>(
            std::filesystem::path(table.filename).stem().string(),
            target.capture)) {
    if (target.pg != nullptr) {
      guts_->mirror.emplace(*target.pg, guts_->stem, table.header);
    }
    writer_.emplace(guts_->stream);
    writer_->writeHeader(table.header);
  }

  Table(const Table &) = delete;
  Table &operator=(const Table &) = delete;
  Table(Table &&) = default;
  Table &operator=(Table &&) = default;

  ~Table() {
    try {
      close();
    } catch (const std::exception &err) {
      std::fprintf(stderr, "exporter::Table: close failed in destructor: %s\n",
                   err.what());
    }
  }

  [[nodiscard]] csv::Writer &writer() noexcept { return *writer_; }

  // Lets a Table stand in wherever a write function takes csv::Writer&.
  operator csv::Writer &() noexcept { return *writer_; }

  // Flushes the rendering and finishes the COPY. Idempotent; also
  // invoked by the destructor (which downgrades errors to stderr).
  void close() {
    if (guts_ == nullptr || guts_->closed) {
      return;
    }
    guts_->closed = true;
    guts_->stream.flush();
    if (guts_->mirror.has_value()) {
      guts_->mirror->close();
    }
  }

private:
  // Heap-held so the stream addresses the Writer and the callback
  // capture stay stable across moves of the Table.
  struct Guts {
    Guts(std::string tableStem, TableCapture *cap)
        : stem(std::move(tableStem)), capture(cap),
          buf([this](const char *data, std::size_t size) {
            if (mirror.has_value()) {
              mirror->put(data, size);
            }
            if (capture != nullptr) {
              capture->put(stem, data, size);
            }
          }),
          stream(&buf) {}

    std::string stem;
    TableCapture *capture;
    std::optional<sinks::TableMirror> mirror;
    io::CallbackStreambuf buf;
    std::ostream stream;
    bool closed = false;
  };

  std::unique_ptr<Guts> guts_;
  std::optional<csv::Writer> writer_;
};

[[nodiscard]] inline Table openTable(const TableTarget &target,
                                     const schema::Table &table) {
  return Table{target, table};
}

} // namespace PhantomLedger::exporter::common
