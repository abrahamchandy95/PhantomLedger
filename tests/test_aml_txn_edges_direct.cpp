//
// tests/test_aml_txn_edges_direct.cpp
//
// CSV retirement arc, step 3b acceptance (skips with 77 when no server
// is reachable; honors PL_TEST_PG). aml-txn-edges is the LAST exporter:
// the two transaction-streamed tables (TRANSACTED, chain labels — open
// across the whole fold on their own connections) plus the ~55 finisher
// tables all flow through common::Table into schema aml_txn_edges with
// the csv_loader tree naming. With this exporter direct, the csv_loader
// mirror pass has no production callers — the loader survives only as
// this gate family's verification oracle.
//
// The gate runs the real binary (DEFAULT engine, live server — the use
// case requires PostgreSQL anyway for its read-back bundle), then loads
// the CSV tree the run just wrote into a scratch schema via csv_loader
// and requires EVERY loaded table to match its direct twin exactly
// (row count + order-insensitive content hash). Generic over the
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

constexpr const char *kScratchSchema = "pl_amltxn_mirror_check";

[[nodiscard]] std::string qualified(Connection &conn, const std::string &schema,
                                    const std::string &table) {
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

  const fs::path outDir = fs::temp_directory_path() / "pl_amltxn_direct";
  const fs::path logPath =
      fs::temp_directory_path() / "pl_amltxn_direct.log";
  fs::remove_all(outDir);

  // Real run, DEFAULT engine, live server (the use case's windowed path
  // needs PostgreSQL for its read-back bundle regardless).
  const std::string cmd =
      std::string{"PL_PG='"} + conninfo + "' \"" + PL_BIN_PATH +
      "\" --usecase aml-txn-edges --population 1000 --days 60"
      " --seed 20260723 --out \"" +
      outDir.string() + "\" > \"" + logPath.string() + "\" 2>&1";

  std::printf("=== aml-txn-edges direct tables vs csv_loader (live "
              "PostgreSQL) ===\n  running binary ...\n");
  std::fflush(stdout);

  if (const int rc = std::system(cmd.c_str()); rc != 0) {
    std::fprintf(stderr, "binary exited %d; log: %s\n", rc, logPath.c_str());
    return 1;
  }

  // Load the SAME files the run wrote into a scratch schema through the
  // csv_loader path the direct tables replace.
  conn->exec(std::string{"DROP SCHEMA IF EXISTS "} +
             conn->escapeIdentifier(kScratchSchema) + " CASCADE");
  static constexpr std::string_view kSkip[] = {"transactions"};
  const auto reports = PhantomLedger::postgres::loadCsvTree(
      *conn, outDir, kScratchSchema, std::span<const std::string_view>{kSkip});
  assert(reports.size() >= 40); // ~57 vertex/edge tables

  bool ok = true;
  std::size_t mismatches = 0;
  for (const auto &report : reports) {
    const auto directCount = count(*conn, "aml_txn_edges", report.table);
    const auto loadedCount = count(*conn, kScratchSchema, report.table);
    const auto directHash = contentHash(*conn, "aml_txn_edges", report.table);
    const auto loadedHash = contentHash(*conn, kScratchSchema, report.table);

    const bool match =
        directCount == loadedCount && directHash == loadedHash;
    if (!match) {
      std::fprintf(stderr,
                   "  %-52s DIVERGES\n    direct: %s rows, hash %s\n"
                   "    loaded: %s rows, hash %s\n",
                   report.table.c_str(), directCount.c_str(),
                   directHash.c_str(), loadedCount.c_str(),
                   loadedHash.c_str());
      ok = false;
      ++mismatches;
    }
  }
  std::printf("  %zu tables compared, %zu diverged\n", reports.size(),
              mismatches);

  conn->exec(std::string{"DROP SCHEMA "} +
             conn->escapeIdentifier(kScratchSchema) + " CASCADE");

  if (!ok) {
    std::fprintf(stderr, "AML-TXN-EDGES DIRECT TABLES DIVERGE FROM FILES\n");
    return 1;
  }

  std::printf("AML-TXN-EDGES DIRECT HOLDS: %zu tables cell-identical "
              "between the direct COPY path and the file-loaded mirror — "
              "every exporter is now direct.\n",
              reports.size());
  return 0;
}
