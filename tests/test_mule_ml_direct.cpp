//
// tests/test_mule_ml_direct.cpp
//
// CSV retirement arc, step 1 acceptance (skips with 77 when no server
// is reachable; honors PL_TEST_PG). The mule-ml exporter is the pilot:
// all four of its tables flow through common::Table, which tees ONE
// rendered byte stream to the CSV file and to a PostgreSQL COPY —
// including transfer.csv, the transaction-scale table held open across
// the whole windowed fold.
//
// The gate runs the real binary (DEFAULT engine, live server), then
// loads the CSV files the run just wrote into a scratch schema via the
// same csv_loader the old mirror pass used, and requires each direct
// table (schema mule_ml) to match its file-loaded twin EXACTLY:
// identical row count and identical order-insensitive content hash
// over every cell of every row. Any divergence in DDL, quoting,
// escaping, or a single cell fails hard.
//
// This is the contract that lets the CSV files become optional later:
// the PostgreSQL tables provably ARE the files.
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

#ifndef PL_BIN_PATH
#error "PL_BIN_PATH must be defined (path to the phantomledger binary)"
#endif

namespace fs = std::filesystem;
using PhantomLedger::postgres::Connection;

namespace {

constexpr const char *kScratchSchema = "pl_mirror_check";

// leaf stems of the four mule-ml tables (loadCsvTree prefixes ml_ready_)
constexpr const char *kStems[] = {"Party", "Transfer_Transaction",
                                  "Account_Device", "Account_IP"};

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

  const fs::path outDir = fs::temp_directory_path() / "pl_mule_ml_direct";
  const fs::path logPath =
      fs::temp_directory_path() / "pl_mule_ml_direct.log";
  fs::remove_all(outDir);

  // Real run, DEFAULT engine, live server: writes the four CSV files
  // AND their direct twins into schema mule_ml.
  const std::string cmd =
      std::string{"PL_PG='"} + conninfo + "' \"" + PL_BIN_PATH +
      "\" --usecase mule-ml --population 1000 --days 60"
      " --seed 20260723 --out \"" +
      outDir.string() + "\" > \"" + logPath.string() + "\" 2>&1";

  std::printf("=== mule-ml direct tables vs csv_loader (live PostgreSQL) "
              "===\n  running binary ...\n");
  std::fflush(stdout);

  if (const int rc = std::system(cmd.c_str()); rc != 0) {
    std::fprintf(stderr, "binary exited %d; log: %s\n", rc, logPath.c_str());
    return 1;
  }

  // Load the SAME files the run wrote into a scratch schema through
  // the csv_loader path the direct tables replace.
  conn->exec(std::string{"DROP SCHEMA IF EXISTS "} +
             conn->escapeIdentifier(kScratchSchema) + " CASCADE");
  const auto reports = PhantomLedger::postgres::loadCsvTree(
      *conn, outDir, kScratchSchema, std::span<const std::string_view>{});
  assert(reports.size() == 4);

  bool ok = true;
  for (const auto *stem : kStems) {
    const std::string table = std::string{"ml_ready_"} + stem;

    const auto directCount = count(*conn, "mule_ml", table);
    const auto loadedCount = count(*conn, kScratchSchema, table);
    const auto directHash = contentHash(*conn, "mule_ml", table);
    const auto loadedHash = contentHash(*conn, kScratchSchema, table);

    const bool match =
        directCount == loadedCount && directHash == loadedHash;
    std::printf("  %-24s %s (%s rows)\n", table.c_str(),
                match ? "MATCHES" : "DIVERGES", directCount.c_str());
    if (!match) {
      std::fprintf(stderr,
                   "  direct: %s rows, hash %s\n  loaded: %s rows, hash %s\n",
                   directCount.c_str(), directHash.c_str(),
                   loadedCount.c_str(), loadedHash.c_str());
      ok = false;
    }
  }

  conn->exec(std::string{"DROP SCHEMA "} +
             conn->escapeIdentifier(kScratchSchema) + " CASCADE");

  if (!ok) {
    std::fprintf(stderr, "MULE-ML DIRECT TABLES DIVERGE FROM FILES\n");
    return 1;
  }

  std::printf("MULE-ML DIRECT HOLDS: 4 tables cell-identical between the "
              "direct COPY path and the file-loaded mirror.\n");
  return 0;
}
