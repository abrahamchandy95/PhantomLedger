//
// tests/test_windowed_aml_txn_edges.cpp
//
// Engine parity for the LAST use case (skips with 77 when no server is
// reachable; honors PL_TEST_PG). aml-txn-edges is the one mode whose
// windowed path REQUIRES PostgreSQL — the derived analytics read the
// streamed corpus back from the transactions table — so unlike the
// serverless test_windowed_e2e, this gate runs the binary against a
// LIVE server:
//
//   leg A  PL_ENGINE=monolithic  (the reference engine, test-only env
//                                 override; there is no --engine flag)
//   leg B  no override           (the DEFAULT selection, which this
//                                 gate thereby pins as auto -> windowed
//                                 for aml-txn-edges under the
//                                 PostgreSQL-required backend policy)
//
// and requires the same "Stream digest" line plus digest-identical
// POSTGRESQL CONTENT: after each leg, every table the run registered
// in the aml_txn_edges schema (~57, discovered via the direct-table
// registry) is digested (md5 + rowcount), together with the streamed
// corpus table in row_seq order; the two legs' digest sets must match
// exactly. PhantomLedger writes no files — table content in PostgreSQL
// IS the output. Leg B exercises the full composition:
// StreamingAmlTxnEdgesExport during the fold, SARs from the
// accumulated fraud groups, readback::buildBundle over the PostgreSQL
// corpus store, and exportFromArtifacts through the shared writer
// seams.
//
// NOTE: both legs write what a real run writes on this server — the
// transactions table (full rewrite), pl_run_manifest/pl_run_spans rows,
// and the aml_txn_edges schema (same precedent as test_postgres, which
// also rewrites the transactions table).
//
// HARD-ENFORCED where it runs.
//

#include "phantomledger/primitives/postgres/connection.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#ifndef PL_BIN_PATH
#error "PL_BIN_PATH must be defined (path to the phantomledger binary)"
#endif

namespace fs = std::filesystem;
using PhantomLedger::postgres::Connection;

namespace {

constexpr const char *kDigestPrefix = "Stream digest: ";
constexpr const char *kEnginePrefix = "Engine: ";

// One digest line per table: "<md5>  <rowcount>  <table>" (same shape
// as the table golden's). An empty `schema` means the connection's
// default (public).
[[nodiscard]] std::string digestLine(Connection &conn,
                                     const std::string &schema,
                                     const std::string &table,
                                     bool orderByRowSeq) {
  const auto qualified =
      schema.empty()
          ? conn.escapeIdentifier(table)
          : conn.escapeIdentifier(schema) + "." + conn.escapeIdentifier(table);
  const std::string order = orderByRowSeq ? "row_seq" : "t::text";
  const auto hash = conn.queryValue(
      "SELECT coalesce(md5(string_agg(t::text, E'\\n' ORDER BY " + order +
      ")), '') FROM " + qualified + " t");
  const auto rows = conn.queryValue("SELECT count(*) FROM " + qualified);
  return hash + "  " + rows + "  " + table;
}

// The run's own declaration of what it wrote: the direct-table
// registry, rewritten per schema by every run (table_mirror.cpp).
[[nodiscard]] std::vector<std::string>
registeredTables(Connection &conn, const std::string &schemaKey) {
  const auto agg = conn.queryValue(
      "SELECT coalesce(string_agg(table_name, E'\\n' ORDER BY table_name), "
      "'') FROM public.pl_direct_tables WHERE schema_name = '" +
      schemaKey + "'");
  std::vector<std::string> out;
  std::stringstream ss{agg};
  std::string line;
  while (std::getline(ss, line)) {
    if (!line.empty()) {
      out.push_back(line);
    }
  }
  return out;
}

// Everything a leg wrote to the server: the corpus stream (row_seq
// order) plus every registered aml_txn_edges table.
[[nodiscard]] std::vector<std::string> digestServerContent(Connection &conn) {
  const auto tables = registeredTables(conn, "aml_txn_edges");
  if (tables.size() < 40) {
    std::fprintf(stderr,
                 "suspiciously few aml_txn_edges tables in the registry "
                 "(%zu); the run did not mirror its tables\n",
                 tables.size());
    std::exit(1);
  }

  std::vector<std::string> lines;
  lines.reserve(tables.size() + 1);
  lines.push_back(
      digestLine(conn, "", "transactions", /*orderByRowSeq=*/true));
  for (const auto &table : tables) {
    lines.push_back(
        digestLine(conn, "aml_txn_edges", table, /*orderByRowSeq=*/false));
  }
  return lines;
}

struct LegOutput {
  std::string streamLine;
  std::string engineLine;
  std::vector<std::string> tableLines;
};

[[nodiscard]] LegOutput runLeg(Connection &conn, const std::string &conninfo,
                               const std::string &label,
                               const std::string &engineEnv,
                               const std::string &expectedEngine) {
  const fs::path logPath =
      fs::temp_directory_path() / ("pl_amltxn_e2e_" + label + ".log");

  const std::string cmd =
      std::string{"PL_PG='"} + conninfo + "' " + engineEnv + "\"" +
      PL_BIN_PATH +
      "\" --usecase aml-txn-edges --population 1000 --days 60"
      " --seed 20260723 > \"" +
      logPath.string() + "\" 2>&1";

  std::printf("  running %s leg ...\n", label.c_str());
  std::fflush(stdout);

  if (const int rc = std::system(cmd.c_str()); rc != 0) {
    std::fprintf(stderr, "binary (%s) exited %d; log: %s\n", label.c_str(), rc,
                 logPath.c_str());
    std::exit(1);
  }

  LegOutput out;

  std::ifstream in{logPath};
  std::string line;
  while (std::getline(in, line)) {
    if (line.rfind(kEnginePrefix, 0) == 0) {
      out.engineLine = line.substr(std::string{kEnginePrefix}.size());
    } else if (line.rfind(kDigestPrefix, 0) == 0) {
      out.streamLine = line.substr(std::string{kDigestPrefix}.size());
    }
  }

  if (out.streamLine.empty()) {
    std::fprintf(stderr, "no '%s' line in %s log: %s\n", kDigestPrefix,
                 label.c_str(), logPath.c_str());
    std::exit(1);
  }
  if (out.engineLine != expectedEngine) {
    std::fprintf(stderr, "%s leg reported engine '%s' (expected '%s')\n",
                 label.c_str(), out.engineLine.c_str(),
                 expectedEngine.c_str());
    std::exit(1);
  }

  out.tableLines = digestServerContent(conn);
  return out;
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

  std::printf("=== aml-txn-edges engine parity (live PostgreSQL) ===\n");
  std::fflush(stdout);

  const auto mono = runLeg(*conn, conninfo, "monolithic",
                           "PL_ENGINE=monolithic ", "monolithic");
  std::printf("  monolithic: %s\n", mono.streamLine.c_str());
  std::fflush(stdout);

  // No engine override: the DEFAULT for aml-txn-edges must resolve to
  // windowed now that PostgreSQL is guaranteed by the backend policy.
  const auto windowed =
      runLeg(*conn, conninfo, "windowed", "", "windowed (auto)");
  std::printf("  windowed:   %s\n", windowed.streamLine.c_str());
  std::fflush(stdout);

  if (mono.streamLine != windowed.streamLine) {
    std::fprintf(stderr,
                 "AML-TXN-EDGES DIVERGES (stream):\n  monolithic: %s\n  "
                 "windowed:   %s\n",
                 mono.streamLine.c_str(), windowed.streamLine.c_str());
    return 1;
  }

  if (mono.tableLines == windowed.tableLines) {
    std::printf("AML-TXN-EDGES HOLDS: identical stream and %zu "
                "digest-identical PostgreSQL tables between engines "
                "(windowed via PostgreSQL read-back, now the default).\n",
                mono.tableLines.size());
    return 0;
  }

  std::fprintf(stderr, "AML-TXN-EDGES DIVERGES (PostgreSQL tables):\n");
  std::size_t shown = 0;
  for (const auto &line : windowed.tableLines) {
    if (std::find(mono.tableLines.begin(), mono.tableLines.end(), line) ==
            mono.tableLines.end() &&
        shown < 10) {
      std::fprintf(stderr, "  windowed-only-or-changed: %s\n", line.c_str());
      ++shown;
    }
  }
  for (const auto &line : mono.tableLines) {
    if (std::find(windowed.tableLines.begin(), windowed.tableLines.end(),
                  line) == windowed.tableLines.end() &&
        shown < 10) {
      std::fprintf(stderr, "  monolithic-only-or-changed: %s\n", line.c_str());
      ++shown;
    }
  }
  return 1;
}
