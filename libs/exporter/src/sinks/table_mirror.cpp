#include "phantomledger/exporter/sinks/table_mirror.hpp"

#include <cstdio>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace PhantomLedger::exporter::sinks {

namespace {

// Same deduplication rule as the csv_loader it replaces, so the DDL of
// a directly-written table can never diverge from a file-loaded one.
[[nodiscard]] std::vector<std::string>
dedupColumns(std::span<const std::string_view> header) {
  std::vector<std::string> cols;
  cols.reserve(header.size());
  std::unordered_set<std::string> used;
  for (const auto col : header) {
    std::string name{col};
    if (used.contains(name)) {
      int n = 2;
      std::string candidate = name + "_" + std::to_string(n);
      while (used.contains(candidate)) {
        candidate = name + "_" + std::to_string(++n);
      }
      name = candidate;
    }
    used.insert(name);
    cols.push_back(std::move(name));
  }
  return cols;
}

// Registry values are internal identifiers ([A-Za-z0-9_] by
// construction); reject anything that would need SQL escaping rather
// than escape it.
[[nodiscard]] const std::string &requirePlain(const std::string &value) {
  if (value.find('\'') != std::string::npos ||
      value.find('\\') != std::string::npos) {
    throw std::logic_error("TableMirror: registry identifier contains a "
                           "quote character: " +
                           value);
  }
  return value;
}

// THE DIRECT-TABLE REGISTRY (CSV retirement step 5a): every table a
// run writes via COPY is recorded in public.pl_direct_tables, so the
// table-digest golden discovers a run's tables from PostgreSQL itself
// instead of from written CSV stems — the last file dependency of the
// falsifiability chain. The FIRST mirror opened for a schema in this
// process rewrites that schema's slate, so the registry always
// describes the LAST run that wrote the schema (stray tables in a
// shared database are never picked up; tables dropped by newer binary
// versions never linger).
void registerDirectTable(postgres::Connection &conn,
                         const std::string &schemaKey,
                         const std::string &table) {
  conn.exec("CREATE TABLE IF NOT EXISTS public.pl_direct_tables ("
            "schema_name text NOT NULL, "
            "table_name text NOT NULL, "
            "PRIMARY KEY (schema_name, table_name))");

  static std::mutex mu;
  static std::set<std::string> clearedSchemas;
  {
    const std::lock_guard<std::mutex> lock{mu};
    if (clearedSchemas.insert(schemaKey).second) {
      conn.exec("DELETE FROM public.pl_direct_tables WHERE schema_name = '" +
                requirePlain(schemaKey) + "'");
    }
  }

  conn.exec("INSERT INTO public.pl_direct_tables (schema_name, table_name) "
            "VALUES ('" +
            requirePlain(schemaKey) + "', '" + requirePlain(table) +
            "') ON CONFLICT DO NOTHING");
}

} // namespace

TableMirror::TableMirror(const PgMirror &target, std::string_view tableStem,
                         std::span<const std::string_view> header)
    : conn_(target.conninfo) {
  const auto table = target.tablePrefix + std::string{tableStem};
  std::string qualified;
  if (!target.schema.empty()) {
    conn_.exec("CREATE SCHEMA IF NOT EXISTS " +
               conn_.escapeIdentifier(target.schema));
    qualified = conn_.escapeIdentifier(target.schema) + "." +
                conn_.escapeIdentifier(table);
  } else {
    qualified = conn_.escapeIdentifier(table);
  }

  std::string ddl;
  for (const auto &col : dedupColumns(header)) {
    ddl += (ddl.empty() ? "" : ", ") + conn_.escapeIdentifier(col) + " text";
  }

  conn_.exec("DROP TABLE IF EXISTS " + qualified);
  conn_.exec("CREATE UNLOGGED TABLE " + qualified + " (" + ddl + ")");

  registerDirectTable(conn_, target.schema.empty() ? "public" : target.schema,
                      table);

  copy_.emplace(conn_, "COPY " + qualified +
                           " FROM STDIN WITH (FORMAT csv, HEADER true)");
}

TableMirror::~TableMirror() {
  try {
    close();
  } catch (const std::exception &err) {
    // Destructors must not throw; a failed COPY finish surfaces loudly
    // but non-fatally. Callers wanting the hard error use close().
    std::fprintf(stderr, "TableMirror: close failed in destructor: %s\n",
                 err.what());
  }
}

void TableMirror::put(const char *data, std::size_t size) {
  if (!copy_.has_value()) {
    throw std::logic_error("TableMirror: put after close");
  }
  copy_->put(data, size);
}

void TableMirror::close() {
  if (copy_.has_value()) {
    copy_->done();
    copy_.reset();
  }
}

} // namespace PhantomLedger::exporter::sinks
