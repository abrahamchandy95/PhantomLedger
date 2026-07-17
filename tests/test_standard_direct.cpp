//
// tests/test_standard_direct.cpp
//
// CSV retirement arc, step 2 acceptance (skips with 77 when no server
// is reachable; honors PL_TEST_PG). The standard exporter is fully
// migrated: every table — entity tables from exportEntities plus the
// streamed has_paid / account_flow_agg — flows through common::Table,
// teeing ONE rendered byte stream to the CSV file and to a direct
// PostgreSQL COPY in the public schema (unprefixed, matching the old
// loadCsvDirectory naming). The ledger CSV (transactions.csv) is the
// deliberate exception: its stem is the streamed corpus table's name,
// so it stays file-only and the canonical stream is never overwritten.
//
// The gate runs the real binary (DEFAULT engine, live server,
// --show-transactions so the exclusion is exercised), then loads the
// CSV files the run just wrote into a scratch schema via the same
// csv_loader the old mirror pass used, and requires EVERY loaded table
// to match its direct twin exactly (row count + order-insensitive
// content hash over every cell). The comparison is generic over the
// loader's report — no hard-coded table list can go stale.
//
// HARD-ENFORCED where it runs.
//

#undef NDEBUG

#include "phantomledger/primitives/postgres/connection.hpp"
#include "phantomledger/primitives/postgres/csv_loader.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#ifndef PL_BIN_PATH
#error "PL_BIN_PATH must be defined (path to the phantomledger binary)"
#endif

namespace fs = std::filesystem;
using PhantomLedger::postgres::Connection;

namespace {

constexpr const char *kScratchSchema = "pl_std_mirror_check";

[[nodiscard]] std::string qualified(Connection &conn, const std::string &schema,
                                    const std::string &table) {
  if (schema.empty()) {
    return conn.escapeIdentifier(table);
  }
  return conn.escapeIdentifier(schema) + "." + conn.escapeIdentifier(table);
}

[[nodiscard]] std::string count(Connection &conn, const std::string &schema,
                                const std::string &table) {
  return conn.queryValue("SELECT count(*) FROM " +
                         qualified(conn, schema, table) + " t");
}

// Order-insensitive canonical content hash over every cell of every
// row (both tables are all-text with identical DDL, so row text is a
// faithful cell-by-cell rendering).
[[nodiscard]] std::string contentHash(Connection &conn,
                                      const std::string &schema,
                                      const std::string &table) {
  return conn.queryValue(
      "SELECT coalesce(md5(string_agg(t::text, E'\\n' ORDER BY t::text)), "
      "'') FROM " +
      qualified(conn, schema, table) + " t");
}

} // namespace

int main() {
  const char *env = std::getenv("PL_TEST_PG");
  const std::string conninfo = env != nullptr ? env : "dbname=phantomledger";

  std::optional<Connection> conn;
  try {
    conn.emplace(conninfo);
  } catch (const std::exception &) {
    std::printf("SKIP: no postgres reachable via '%s'\n", conninfo.c_str());
    return 77;
  }

  const fs::path outDir = fs::temp_directory_path() / "pl_standard_direct";
  const fs::path logPath =
      fs::temp_directory_path() / "pl_standard_direct.log";
  fs::remove_all(outDir);

  // Real run, DEFAULT engine, live server, --show-transactions so the
  // file-only ledger exception is exercised alongside the direct tables.
  const std::string cmd =
      std::string{"PL_PG='"} + conninfo + "' \"" + PL_BIN_PATH +
      "\" --usecase standard --population 1000 --days 60"
      " --seed 20260723 --show-transactions --out \"" +
      outDir.string() + "\" > \"" + logPath.string() + "\" 2>&1";

  std::printf("=== standard direct tables vs csv_loader (live PostgreSQL) "
              "===\n  running binary ...\n");
  std::fflush(stdout);

  if (const int rc = std::system(cmd.c_str()); rc != 0) {
    std::fprintf(stderr, "binary exited %d; log: %s\n", rc, logPath.c_str());
    return 1;
  }

  // Load the SAME files the run wrote into a scratch schema through the
  // csv_loader path the direct tables replace — skipping the ledger CSV
  // exactly like the production mirror always did.
  conn->exec(std::string{"DROP SCHEMA IF EXISTS "} +
             conn->escapeIdentifier(kScratchSchema) + " CASCADE");
  static constexpr std::string_view kSkip[] = {"transactions"};
  const auto reports = PhantomLedger::postgres::loadCsvDirectory(
      *conn, outDir, std::span<const std::string_view>{kSkip},
      kScratchSchema);
  assert(reports.size() >= 10); // entity + ER + aggregate tables

  bool ok = true;
  for (const auto &report : reports) {
    const auto directCount = count(*conn, "", report.table);
    const auto loadedCount = count(*conn, kScratchSchema, report.table);
    const auto directHash = contentHash(*conn, "", report.table);
    const auto loadedHash = contentHash(*conn, kScratchSchema, report.table);

    const bool match =
        directCount == loadedCount && directHash == loadedHash;
    std::printf("  %-28s %s (%s rows)\n", report.table.c_str(),
                match ? "MATCHES" : "DIVERGES", directCount.c_str());
    if (!match) {
      std::fprintf(stderr,
                   "  direct: %s rows, hash %s\n  loaded: %s rows, hash %s\n",
                   directCount.c_str(), directHash.c_str(),
                   loadedCount.c_str(), loadedHash.c_str());
      ok = false;
    }
  }

  // The canonical stream must be untouched: transactions carries its
  // bookkeeping columns, which the ledger CSV does not have — if the
  // dump had been mirrored over it, this column would not exist.
  const auto rowSeqMax = conn->queryValue(
      "SELECT coalesce(max(row_seq), 0) FROM transactions");
  const bool streamIntact = rowSeqMax != "0";
  if (!streamIntact) {
    std::fprintf(stderr,
                 "streamed transactions table lost its row_seq content\n");
    ok = false;
  }

  conn->exec(std::string{"DROP SCHEMA "} +
             conn->escapeIdentifier(kScratchSchema) + " CASCADE");

  if (!ok) {
    std::fprintf(stderr, "STANDARD DIRECT TABLES DIVERGE FROM FILES\n");
    return 1;
  }

  std::printf("STANDARD DIRECT HOLDS: %zu tables cell-identical between "
              "the direct COPY path and the file-loaded mirror; the "
              "streamed corpus table is intact.\n",
              reports.size());
  return 0;
}
