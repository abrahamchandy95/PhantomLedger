#pragma once
/*
  Direct-to-PostgreSQL table writing. A TableMirror receives the EXACT
  bytes of one CSV table, header line included, and streams them into a
  schema table via COPY ... WITH (FORMAT csv, HEADER true). DDL comes from
  the schema::Table descriptor: all-text columns, DROP+CREATE per run,
  duplicate column names deduplicated, <prefix><file stem> table naming.
  Because THE COPY PAYLOAD IS THE FILE BYTES, CSV/PostgreSQL parity is a
  structural property, pinned cell-by-cell by test_mule_ml_direct.

  ONE TableMirror OWNS ONE Connection: a connection can host only one COPY
  at a time, and streamed tables (e.g. mule-ml's transfer table) stay open
  across the whole fold.

  THE DIRECT-TABLE REGISTRY: every mirrored table also registers itself in
  public.pl_direct_tables (schema_name, table_name). The first mirror a
  process opens for a schema rewrites that schema's slate, so the registry
  always lists exactly the tables the LAST run wrote — which is how the
  table-digest golden discovers a run's tables without depending on
  files.
 */

#include "phantomledger/primitives/postgres/connection.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace PhantomLedger::exporter::sinks {

/* Where direct tables land. tablePrefix reproduces the loader tree naming
 * (<subdirs>_<stem>), e.g. schema "mule_ml", prefix "ml_ready_". */
struct PgMirror {
  std::string conninfo;
  std::string schema;
  std::string tablePrefix;
};

class TableMirror {
public:
  /* Creates <schema>.<tablePrefix><tableStem> (DROP+CREATE, UNLOGGED,
   * all-text columns from `header`), records it in the direct-table
   * registry, and opens the COPY. */
  TableMirror(const PgMirror &target, std::string_view tableStem,
              std::span<const std::string_view> header);

  TableMirror(const TableMirror &) = delete;
  TableMirror &operator=(const TableMirror &) = delete;
  TableMirror(TableMirror &&) = delete;
  TableMirror &operator=(TableMirror &&) = delete;

  ~TableMirror();

  /* Exact CSV bytes, header line included (COPY runs with HEADER true). */
  void put(const char *data, std::size_t size);

  /* Finishes the COPY. Idempotent; also invoked by the destructor. */
  void close();

private:
  postgres::Connection conn_;
  std::optional<postgres::CopyIn> copy_;
};

} // namespace PhantomLedger::exporter::sinks
