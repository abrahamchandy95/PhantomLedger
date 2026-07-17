//
// tests/test_table_golden.cpp
//
// CSV retirement arc, step 4: the TABLE-DIGEST GOLDEN — falsifiability
// transferred from CSV bytes to PostgreSQL content (skips with 77 when
// no server is reachable; honors PL_TEST_PG).
//
// Runs the standard use case with the SAME config as the CSV golden
// (pop 2000, 60 days, seed 3405691582, --show-transactions), then
// digests every table the run wrote in the public schema:
//
//   transactions      md5 over row text in row_seq order — the corpus,
//                     bookkeeping columns included (this is the load-
//                     bearing corpus pin the CSV golden gets from
//                     transactions.csv)
//   direct tables     discovered from the run's own CSV stems (immune
//                     to stray tables in a shared database), md5 over
//                     row text in canonical ORDER BY t::text
//
// First live run captures tests/golden_tables.md5 (reported as SKIP so
// capture is explicit — the baseline belongs in git, per-server like
// the CSV golden is per-toolchain); every later run enforces exact
// equality. Delete the baseline to re-pin after an intentional model
// change — the SAME re-pin discipline as golden_run.b2sum, and any
// re-pin must update BOTH baselines in the same commit.
//
// PARALLEL-RUN POLICY: the CSV golden remains the authoritative
// falsifier until this baseline has survived at least one full round
// of intentional model changes with equal catch-power. Only then may
// CSV emission become optional (step 5).
//

#undef NDEBUG

#include "phantomledger/primitives/postgres/connection.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#ifndef PL_BIN_PATH
#error "PL_BIN_PATH must be defined (path to the phantomledger binary)"
#endif
#ifndef PL_TABLE_BASELINE
#error "PL_TABLE_BASELINE must be defined (path to the baseline file)"
#endif

namespace fs = std::filesystem;
using PhantomLedger::postgres::Connection;

namespace {

// One digest line per table: "<md5>  <rowcount>  <table>".
[[nodiscard]] std::string digestLine(Connection &conn,
                                     const std::string &table,
                                     bool orderByRowSeq) {
  const auto qualified = conn.escapeIdentifier(table);
  const std::string order = orderByRowSeq ? "row_seq" : "t::text";
  const auto hash = conn.queryValue(
      "SELECT coalesce(md5(string_agg(t::text, E'\\n' ORDER BY " + order +
      ")), '') FROM " + qualified + " t");
  const auto rows = conn.queryValue("SELECT count(*) FROM " + qualified);
  return hash + "  " + rows + "  " + table;
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

  const fs::path outDir = fs::temp_directory_path() / "pl_table_golden";
  const fs::path logPath = fs::temp_directory_path() / "pl_table_golden.log";
  fs::remove_all(outDir);

  // The CSV golden's exact config, so the two goldens pin the SAME
  // corpus and their catch-power is directly comparable.
  const std::string cmd =
      std::string{"PL_PG='"} + conninfo + "' \"" + PL_BIN_PATH +
      "\" --population 2000 --days 60"
      " --seed 3405691582 --show-transactions --out \"" +
      outDir.string() + "\" > \"" + logPath.string() + "\" 2>&1";

  std::printf("=== table-digest golden (live PostgreSQL) ===\n"
              "  running binary ...\n");
  std::fflush(stdout);

  if (const int rc = std::system(cmd.c_str()); rc != 0) {
    std::fprintf(stderr, "binary exited %d; log: %s\n", rc, logPath.c_str());
    return 1;
  }

  // Table discovery from the run's own output: every CSV stem is a
  // direct table in the public schema (the ledger dump's stem is the
  // stream table, handled explicitly with row_seq ordering).
  std::vector<std::string> tables;
  for (const auto &entry : fs::directory_iterator(outDir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".csv") {
      continue;
    }
    const auto stem = entry.path().stem().string();
    if (stem == "transactions") {
      continue; // file-only ledger dump; the stream table is below
    }
    tables.push_back(stem);
  }
  assert(tables.size() >= 10);
  std::sort(tables.begin(), tables.end());

  std::vector<std::string> lines;
  lines.reserve(tables.size() + 1);
  lines.push_back(digestLine(*conn, "transactions", /*orderByRowSeq=*/true));
  for (const auto &table : tables) {
    lines.push_back(digestLine(*conn, table, /*orderByRowSeq=*/false));
  }

  const fs::path baseline{PL_TABLE_BASELINE};
  if (!fs::exists(baseline)) {
    std::ofstream out{baseline};
    if (!out) {
      std::fprintf(stderr,
                   "table-golden: cannot open baseline for write: %s\n",
                   baseline.c_str());
      return 1;
    }
    for (const auto &line : lines) {
      out << line << '\n';
    }
    out.flush();
    if (!out) {
      std::fprintf(stderr, "table-golden: baseline write FAILED: %s\n",
                   baseline.c_str());
      return 1;
    }
    std::printf("table-golden: baseline captured (%zu tables) at %s\n",
                lines.size(), baseline.c_str());
    return 77; // reported as SKIP: capture is explicit, never a pass
  }

  std::vector<std::string> expected;
  {
    std::ifstream in{baseline};
    std::string line;
    while (std::getline(in, line)) {
      if (!line.empty()) {
        expected.push_back(line);
      }
    }
  }

  if (expected == lines) {
    std::printf("table-golden: %zu tables digest-identical to baseline "
                "(corpus pinned via transactions in row_seq order)\n",
                lines.size());
    return 0;
  }

  std::fprintf(stderr, "table-golden: POSTGRESQL CONTENT DIVERGES FROM "
                       "BASELINE\n");
  std::size_t shown = 0;
  for (const auto &line : lines) {
    if (std::find(expected.begin(), expected.end(), line) == expected.end() &&
        shown < 10) {
      std::fprintf(stderr, "  changed-or-new: %s\n", line.c_str());
      ++shown;
    }
  }
  for (const auto &line : expected) {
    if (std::find(lines.begin(), lines.end(), line) == lines.end() &&
        shown < 10) {
      std::fprintf(stderr, "  was-in-baseline: %s\n", line.c_str());
      ++shown;
    }
  }
  std::fprintf(stderr,
               "if this change was intentional, delete %s and rerun to "
               "re-pin — and re-pin the CSV golden in the SAME commit\n",
               baseline.c_str());
  return 1;
}
