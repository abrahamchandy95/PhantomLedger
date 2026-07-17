#pragma once
//
// phantomledger/exporter/sinks/table_mirror.hpp
//
// Direct-to-PostgreSQL table writing — step 1 of the CSV retirement
// arc. A TableMirror receives the EXACT bytes of one CSV table (header
// line included) and streams them into a schema table via
// COPY ... WITH (FORMAT csv, HEADER true), with DDL derived from the
// schema::Table descriptor using the same conventions as the csv_loader
// mirror it replaces (all-text columns, DROP+CREATE per run, duplicate
// column names deduplicated, <prefix><file stem> table naming). Because
// the COPY payload IS the file bytes, CSV/PostgreSQL parity is a
// structural property, pinned cell-by-cell by test_mule_ml_direct.
//
// One TableMirror owns one Connection: a connection can host only one
// COPY at a time, and streamed tables (e.g. mule-ml's transfer table)
// stay open across the whole fold.
//

#include "phantomledger/primitives/postgres/connection.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace PhantomLedger::exporter::sinks {

// Where direct tables land. tablePrefix reproduces the csv_loader tree
// naming (<subdirs>_<stem>), e.g. schema "mule_ml", prefix "ml_ready_".
struct PgMirror {
  std::string conninfo;
  std::string schema;
  std::string tablePrefix;
};

class TableMirror {
public:
  // Creates <schema>.<tablePrefix><tableStem> (DROP+CREATE, UNLOGGED,
  // all-text columns from `header`) and opens the COPY.
  TableMirror(const PgMirror &target, std::string_view tableStem,
              std::span<const std::string_view> header);

  TableMirror(const TableMirror &) = delete;
  TableMirror &operator=(const TableMirror &) = delete;
  TableMirror(TableMirror &&) = delete;
  TableMirror &operator=(TableMirror &&) = delete;

  ~TableMirror();

  // Exact CSV bytes, header line included (COPY runs with HEADER true).
  void put(const char *data, std::size_t size);

  // Finishes the COPY. Idempotent; also invoked by the destructor.
  void close();

private:
  postgres::Connection conn_;
  std::optional<postgres::CopyIn> copy_;
};

} // namespace PhantomLedger::exporter::sinks
