//
// tests/test_table_golden.cpp
//
// CSV retirement arc, step 4 + the FRAUD-VISIBLE PIN: the TABLE-DIGEST
// GOLDEN — falsifiability transferred from CSV bytes to PostgreSQL
// content (skips with 77 when no server is reachable; honors
// PL_TEST_PG). TWO sections, each with its own baseline file:
//
//   SECTION "standard" — the CSV golden's exact config (standard use
//   case, pop 2000, 60 days, seed 3405691582, --show-transactions),
//   digesting the corpus stream (row_seq order, bookkeeping columns
//   included — the load-bearing corpus pin) plus every direct table
//   the run wrote in the public schema, discovered from the run's own
//   CSV stems (immune to stray tables in a shared database).
//   Baseline: tests/golden_tables.md5.
//
//   SECTION "fraud" — aml-txn-edges at a FRAUD-DENSE config (pop
//   10000, 60 days, seed 7; probe-verified 2026-07-18: 5 rings,
//   structuring rows spanning the widened F1 band, derived shell
//   scores, SARs, alerts, CTRs, cases). This section exists because
//   the standard-config goldens are structurally blind to fraud-LABEL
//   tables (the fraud-audit-2026-07 coverage finding): ShellAccount,
//   Sar and the SAR edges, Alert/Ctr/Disposition/InvestigationCase
//   exist only in the aml use cases. The gate HARD-REQUIRES those
//   tables to be present in the pin, and also pins the fraud-dense
//   corpus itself (the shared public.transactions stream, which this
//   section's run overwrites — hence it runs AFTER the standard
//   section is digested). Baseline: tests/golden_tables_aml.md5.
//
// First live run captures a missing baseline (reported as SKIP so
// capture is explicit — a captured baseline belongs in git IMMEDIATELY,
// per-server like the CSV golden is per-toolchain); every later run
// enforces exact equality. Delete a baseline to re-pin after an
// intentional model change, and re-pin EVERY baseline the change
// touches in the SAME named commit.
//
// PROMOTION: the fraud section's first intentional-model-change round
// is the table golden's promotion trial (the 2026-07 batch's trial was
// vacuous — the standard config never sees fraud labels).
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
#error "PL_TABLE_BASELINE must be defined (path to the standard baseline)"
#endif
#ifndef PL_TABLE_BASELINE_AML
#error "PL_TABLE_BASELINE_AML must be defined (path to the fraud baseline)"
#endif

namespace fs = std::filesystem;
using PhantomLedger::postgres::Connection;

namespace {

// One digest line per table: "<md5>  <rowcount>  <table>". An empty
// `schema` means the connection's default (public).
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

[[nodiscard]] bool runBinary(const std::string &args, const fs::path &outDir,
                             const fs::path &logPath,
                             const std::string &conninfo) {
  fs::remove_all(outDir);
  const std::string cmd = std::string{"PL_PG='"} + conninfo + "' \"" +
                          PL_BIN_PATH + "\" " + args + " --out \"" +
                          outDir.string() + "\" > \"" + logPath.string() +
                          "\" 2>&1";
  if (const int rc = std::system(cmd.c_str()); rc != 0) {
    std::fprintf(stderr, "binary exited %d; log: %s\n", rc, logPath.c_str());
    return false;
  }
  return true;
}

enum class Section : int { pass, captured, diverged };

// Capture-if-missing / enforce-if-present, shared by both sections.
[[nodiscard]] Section applyBaseline(const fs::path &baseline,
                                    const std::vector<std::string> &lines,
                                    const char *name) {
  if (!fs::exists(baseline)) {
    std::ofstream out{baseline};
    if (!out) {
      std::fprintf(stderr,
                   "table-golden[%s]: cannot open baseline for write: %s\n",
                   name, baseline.c_str());
      return Section::diverged;
    }
    for (const auto &line : lines) {
      out << line << '\n';
    }
    out.flush();
    if (!out) {
      std::fprintf(stderr, "table-golden[%s]: baseline write FAILED: %s\n",
                   name, baseline.c_str());
      return Section::diverged;
    }
    std::printf("table-golden[%s]: baseline captured (%zu tables) at %s — "
                "commit it in the capture round, not later\n",
                name, lines.size(), baseline.c_str());
    return Section::captured; // reported as SKIP: capture is never a pass
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
    std::printf("table-golden[%s]: %zu tables digest-identical to baseline\n",
                name, lines.size());
    return Section::pass;
  }

  std::fprintf(stderr,
               "table-golden[%s]: POSTGRESQL CONTENT DIVERGES FROM "
               "BASELINE\n",
               name);
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
               "re-pin — and re-pin every golden the change touches in the "
               "SAME named commit\n",
               baseline.c_str());
  return Section::diverged;
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

  const fs::path tmp = fs::temp_directory_path();

  // ------------------------------------------------------------------
  // SECTION "standard": the CSV golden's exact config, so the two
  // goldens pin the SAME corpus and their catch-power is directly
  // comparable.
  // ------------------------------------------------------------------
  const fs::path outStd = tmp / "pl_table_golden";
  std::printf("=== table-digest golden (live PostgreSQL) ===\n"
              "  [standard] running binary ...\n");
  std::fflush(stdout);
  if (!runBinary("--population 2000 --days 60 --seed 3405691582"
                 " --show-transactions",
                 outStd, tmp / "pl_table_golden.log", conninfo)) {
    return 1;
  }

  // Table discovery from the run's own output: every CSV stem is a
  // direct table in the public schema (the ledger dump's stem is the
  // stream table, handled explicitly with row_seq ordering).
  std::vector<std::string> stdTables;
  for (const auto &entry : fs::directory_iterator(outStd)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".csv") {
      continue;
    }
    const auto stem = entry.path().stem().string();
    if (stem == "transactions") {
      continue; // file-only ledger dump; the stream table is below
    }
    stdTables.push_back(stem);
  }
  assert(stdTables.size() >= 10);
  std::sort(stdTables.begin(), stdTables.end());

  std::vector<std::string> stdLines;
  stdLines.reserve(stdTables.size() + 1);
  stdLines.push_back(
      digestLine(*conn, "", "transactions", /*orderByRowSeq=*/true));
  for (const auto &table : stdTables) {
    stdLines.push_back(digestLine(*conn, "", table, /*orderByRowSeq=*/false));
  }

  // ------------------------------------------------------------------
  // SECTION "fraud": aml-txn-edges at the fraud-dense probe config.
  // Runs strictly AFTER the standard section is digested — this run
  // overwrites the shared public.transactions stream table.
  // ------------------------------------------------------------------
  const fs::path outFraud = tmp / "pl_table_golden_aml";
  std::printf("  [fraud] running binary (aml-txn-edges, pop 10000) ...\n");
  std::fflush(stdout);
  if (!runBinary("--usecase aml-txn-edges --population 10000 --days 60"
                 " --seed 7",
                 outFraud, tmp / "pl_table_golden_aml.log", conninfo)) {
    return 1;
  }

  // Discovery mirrors TableMirror naming: schema aml_txn_edges, table
  // "aml_txn_edges_<subdir>_<stem>" with the stem verbatim.
  std::vector<std::string> fraudTables;
  for (const char *sub : {"vertices", "edges"}) {
    const fs::path dir = outFraud / "aml_txn_edges" / sub;
    for (const auto &entry : fs::directory_iterator(dir)) {
      if (!entry.is_regular_file() || entry.path().extension() != ".csv") {
        continue;
      }
      fraudTables.push_back(std::string{"aml_txn_edges_"} + sub + "_" +
                            entry.path().stem().string());
    }
  }
  assert(fraudTables.size() >= 40);
  std::sort(fraudTables.begin(), fraudTables.end());

  // The whole point of this section: the fraud-LABEL tables the
  // standard config never produces MUST be under the pin.
  for (const char *required : {"aml_txn_edges_vertices_ShellAccount",
                               "aml_txn_edges_vertices_Sar",
                               "aml_txn_edges_vertices_Alert",
                               "aml_txn_edges_vertices_Ctr",
                               "aml_txn_edges_vertices_InvestigationCase"}) {
    if (std::find(fraudTables.begin(), fraudTables.end(),
                  std::string{required}) == fraudTables.end()) {
      std::fprintf(stderr,
                   "table-golden[fraud]: required fraud-label table missing "
                   "from the run's output: %s\n",
                   required);
      return 1;
    }
  }

  std::vector<std::string> fraudLines;
  fraudLines.reserve(fraudTables.size() + 1);
  // The fraud-dense corpus itself (structuring amounts included) —
  // F1-sensitive where the standard config is blind.
  fraudLines.push_back(
      digestLine(*conn, "", "transactions", /*orderByRowSeq=*/true));
  for (const auto &table : fraudTables) {
    fraudLines.push_back(
        digestLine(*conn, "aml_txn_edges", table, /*orderByRowSeq=*/false));
  }

  // ------------------------------------------------------------------
  // Baselines: capture-if-missing (SKIP), enforce-if-present.
  // ------------------------------------------------------------------
  const auto stdResult =
      applyBaseline(fs::path{PL_TABLE_BASELINE}, stdLines, "standard");
  const auto fraudResult =
      applyBaseline(fs::path{PL_TABLE_BASELINE_AML}, fraudLines, "fraud");

  if (stdResult == Section::diverged || fraudResult == Section::diverged) {
    return 1;
  }
  if (stdResult == Section::captured || fraudResult == Section::captured) {
    return 77;
  }
  std::printf("table-golden: both sections pinned (corpus via row_seq; "
              "fraud labels via the aml section)\n");
  return 0;
}
