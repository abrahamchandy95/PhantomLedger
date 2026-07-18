#pragma once
//
// phantomledger/exporter/common/table.hpp
//
// The direct-table twin of common::openTable — step 1 of the CSV
// retirement arc. openTable(TableTarget, schema::Table) returns a
// common::Table whose csv::Writer renders each row ONCE into a single
// byte stream that is tee'd to the CSV file and (when the target's
// PgMirror is armed) to a PostgreSQL COPY. One rendering, two
// destinations: the table content in PostgreSQL is byte-identical to
// the file by construction, which is what lets the CSV files become
// optional without a falsifiability gap.
//
// CSV retirement step 5b: an EMPTY target.dir means "no file leg" —
// the table renders once and streams only into the mirror (or, with
// no mirror either, nowhere: a legal null sink for file-less
// serverless runs). Exporters that compose subdirectories must pass a
// TRULY EMPTY dir when files are disabled — an empty base composed
// with a subdir ("aml/vertices") is a NON-empty relative path and
// would silently write into the working directory.
//
// A Table converts implicitly to csv::Writer&, so every existing
// write function (writePartyRows, writeTransferRows, ...) works
// unchanged.
//

#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/exporter/schema.hpp"
#include "phantomledger/exporter/sinks/table_mirror.hpp"
#include "phantomledger/primitives/io/callback_streambuf.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <utility>

namespace PhantomLedger::exporter::common {

// Where an exporter's tables go: the directory when non-empty (empty
// => no file is written); additionally a PostgreSQL mirror when `pg`
// is set. Exporters receive this from their Options/Config and never
// decide backend policy themselves.
struct TableTarget {
  std::filesystem::path dir;
  const sinks::PgMirror *pg = nullptr; // nullptr => no direct table
};

class Table {
public:
  Table(const TableTarget &target, const schema::Table &table)
      : guts_(std::make_unique<Guts>(
            target.dir.empty()
                ? std::optional<std::filesystem::path>{}
                : std::optional<std::filesystem::path>{
                      target.dir / std::filesystem::path(table.filename)})) {
    if (!target.dir.empty() && !guts_->file) {
      throw std::runtime_error(
          "exporter: cannot open table file " +
          (target.dir / std::filesystem::path(table.filename)).string());
    }
    if (target.pg != nullptr) {
      const auto stem =
          std::filesystem::path(table.filename).stem().string();
      guts_->mirror.emplace(*target.pg, stem, table.header);
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

  // Flushes the tee and finishes the COPY. Idempotent; also invoked by
  // the destructor (which downgrades errors to stderr).
  void close() {
    if (guts_ == nullptr || guts_->closed) {
      return;
    }
    guts_->closed = true;
    guts_->stream.flush();
    if (guts_->mirror.has_value()) {
      guts_->mirror->close();
    }
    if (guts_->file.is_open()) {
      guts_->file.close();
    }
  }

private:
  // Heap-held so the stream addresses the Writer and the callback
  // capture stay stable across moves of the Table.
  struct Guts {
    explicit Guts(const std::optional<std::filesystem::path> &path)
        : buf([this](const char *data, std::size_t size) {
            if (file.is_open()) {
              file.write(data, static_cast<std::streamsize>(size));
            }
            if (mirror.has_value()) {
              mirror->put(data, size);
            }
          }),
          stream(&buf) {
      if (path.has_value()) {
        file.open(*path, std::ios::binary);
      }
    }

    std::ofstream file;
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
