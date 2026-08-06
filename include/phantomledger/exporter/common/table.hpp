#pragma once
/*
  Direct-table writing. A common::Table renders each row ONCE through its
  csv::Writer into a single byte stream that feeds the table's PostgreSQL
  COPY (when the target's PgMirror is armed). PhantomLedger writes no
  files: THE RENDERED BYTES ARE THE COPY PAYLOAD, so what lands in
  PostgreSQL is the writer's output by construction — there is no second
  rendering to drift.

  With no mirror the rendering goes nowhere: a legal null sink, which the
  serverless PL_FILE_ONLY harness relies on to yield only the corpus stream
  digest.

  A Table converts implicitly to csv::Writer&, so every write function
  (writePartyRows, writeTransferRows, ...) takes one unchanged.
 */

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
#include <stdexcept>
#include <string>
#include <string_view>

namespace PhantomLedger::exporter::common {

/* TEST INFRASTRUCTURE — production code never installs one. Receives the
 * exact rendered bytes of every table opened against a target that carries
 * it, keyed by the table's filename stem (e.g. "Party", "SAR"). Streamed
 * tables deliver their bytes incrementally across the whole fold; this is
 * the serverless seam test_pipeline_e2e uses to pin each exporter's table
 * set, there being no files to inspect. */
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

/* Where an exporter's tables go: a direct PostgreSQL table when `pg` is
 * set, plus the test capture when one is installed. Exporters receive this
 * from their Options/Config and never decide backend policy themselves. */
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
      /* std::fprintf, NOT std::print: this runs inside a destructor's catch
       * handler, where a throwing formatter would terminate the process
       * instead of reporting the COPY failure. */
      std::fprintf(stderr, "exporter::Table: close failed in destructor: %s\n",
                   err.what());
    }
  }

  [[nodiscard]] csv::Writer &writer() noexcept { return *writer_; }

  /* Lets a Table stand in wherever a write function takes csv::Writer&. */
  operator csv::Writer &() noexcept { return *writer_; }

  /* Flushes the rendering and finishes the COPY. Idempotent; also invoked
   * by the destructor, which downgrades errors to stderr. */
  void close() {
    if (guts_ == nullptr || guts_->closed) {
      return;
    }
    guts_->closed = true;
    guts_->stream.flush();
    if (!guts_->stream) {
      throw std::runtime_error("exporter::Table: rendering failed for table " +
                               guts_->stem);
    }
    if (guts_->mirror.has_value()) {
      guts_->mirror->close();
    }
  }

private:
  /* Heap-held so the stream addresses the Writer and the callback capture
   * stay stable across moves of the Table. */
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
          stream(&buf) {
      /* std::ostream otherwise converts callback/streambuf exceptions into
       * a silent badbit. COPY and capture failures MUST reach explicit
       * close() — preferably the write that encountered them — or a run
       * manifest could be marked complete after committing a truncated
       * table. */
      stream.exceptions(std::ios::badbit | std::ios::failbit);
    }

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
