//
// tests/test_run_golden.cpp
//
// End-to-end corpus fingerprint. The binary, run serverless with the
// pinned config, must print the exact
//
//   Stream digest: <hex>  rows: <N>
//
// line recorded in the baseline — the Golden sink's BLAKE2b over the
// rendered corpus stream, in row_seq order. The run writes NO files
// (PostgreSQL-only product; no --out): PL_FILE_ONLY=1 is the sanctioned
// harness escape from the fail-fast PostgreSQL requirement, and PL_PG
// additionally pins an unreachable target as defense in depth — if the
// escape ever breaks, this test fails loudly instead of clobbering the
// developer's real mirror.
//
// First run on a machine captures a per-toolchain baseline (reported as
// SKIP so capture is explicit, never a silent pass). Delete
// tests/golden_run.b2sum to re-pin after an intentional behavior change
// or a toolchain upgrade; the baseline belongs in git. Table bytes are
// pinned separately by the live-PostgreSQL table-digest golden
// (test_table_golden); this gate keeps the corpus pinned even with no
// server anywhere.
//
// A digest pins whatever it is given, an absurd corpus included
// (cash-hub-defect-2026-08), so the run's own summary lines must pass a
// domain predicate BEFORE the digest is compared or captured: the world
// holds the pinned population and at least one account per person, the
// summary counts the rows the sink digested, and the corpus carries both
// classes (0 < fraud rows < rows). A failing predicate is a failure even
// on the capture run, so a re-pin can never record an empty or fraud-free
// stream.
//

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#ifndef PL_BIN_PATH
#error "PL_BIN_PATH must be defined (path to the phantomledger binary)"
#endif
#ifndef PL_GOLDEN_BASELINE
#error "PL_GOLDEN_BASELINE must be defined (path to the baseline file)"
#endif

namespace fs = std::filesystem;

namespace {

constexpr const char *kDigestPrefix = "Stream digest: ";
constexpr unsigned long long kPopulation = 2000;

} // namespace

int main() {
  const fs::path logPath = fs::temp_directory_path() / "pl_run_golden.log";

  const std::string cmd = std::string{"PL_FILE_ONLY=1 "
                                      "PL_PG='host=127.0.0.1 port=9 "
                                      "dbname=pl_disabled' \""} +
                          PL_BIN_PATH +
                          "\" --population " + std::to_string(kPopulation) +
                          " --days 60 --seed 3405691582 > \"" +
                          logPath.string() + "\" 2>&1";
  if (const int rc = std::system(cmd.c_str()); rc != 0) {
    std::fprintf(stderr, "binary exited %d; log: %s\n", rc, logPath.c_str());
    return 1;
  }

  std::string digestLine;
  unsigned long long people = 0;
  unsigned long long accounts = 0;
  unsigned long long summaryRows = 0;
  unsigned long long fraudRows = 0;
  int summaryFields = 0;
  {
    std::ifstream in{logPath};
    std::string line;
    while (std::getline(in, line)) {
      if (line.rfind(kDigestPrefix, 0) == 0) {
        digestLine = line;
      }
      if (const auto at = line.find("People: "); at != std::string::npos) {
        summaryFields += std::sscanf(line.c_str() + at,
                                     "People: %llu  Accounts: %llu", &people,
                                     &accounts);
      }
      if (const auto at = line.find("Transactions: ");
          at != std::string::npos) {
        summaryFields += std::sscanf(line.c_str() + at,
                                     "Transactions: %llu  Fraud rows: %llu",
                                     &summaryRows, &fraudRows);
      }
    }
  }
  if (digestLine.empty()) {
    std::fprintf(stderr, "golden-run: no '%s' line in the run log: %s\n",
                 kDigestPrefix, logPath.c_str());
    return 1;
  }

  unsigned long long digestRows = 0;
  const auto rowsAt = digestLine.find("rows: ");
  const bool digestRowsRead =
      rowsAt != std::string::npos &&
      std::sscanf(digestLine.c_str() + rowsAt, "rows: %llu", &digestRows) == 1;
  if (summaryFields != 4 || !digestRowsRead || people != kPopulation ||
      accounts < people || summaryRows != digestRows || fraudRows == 0 ||
      fraudRows >= summaryRows) {
    std::fprintf(stderr,
                 "golden-run: DOMAIN PREDICATE FAILED (summary fields %d of "
                 "4; people %llu, want %llu; accounts %llu; summary rows %llu "
                 "against %llu digested; fraud rows %llu, want 0 < fraud < "
                 "rows); log: %s\n",
                 summaryFields, people, kPopulation, accounts, summaryRows,
                 digestRows, fraudRows, logPath.c_str());
    return 1;
  }
  std::printf("golden-run: domain predicate holds (people %llu, accounts "
              "%llu, rows %llu, fraud rows %llu)\n",
              people, accounts, summaryRows, fraudRows);

  const fs::path baseline{PL_GOLDEN_BASELINE};
  if (!fs::exists(baseline)) {
    std::ofstream out{baseline};
    if (!out) {
      std::fprintf(stderr, "golden-run: cannot open baseline for write: %s\n",
                   baseline.c_str());
      return 1;
    }
    out << digestLine << '\n';
    out.flush();
    if (!out) {
      std::fprintf(stderr, "golden-run: baseline write FAILED: %s\n",
                   baseline.c_str());
      return 1;
    }
    std::printf("golden-run: baseline captured at %s\n  %s\n",
                baseline.c_str(), digestLine.c_str());
    return 77; // reported as SKIP: capture is explicit, never a pass
  }

  std::string expected;
  {
    std::ifstream in{baseline};
    std::string line;
    while (std::getline(in, line)) {
      if (!line.empty()) {
        expected = line;
        break;
      }
    }
  }
  if (expected.empty()) {
    std::fprintf(stderr, "golden-run: baseline is empty: %s\n",
                 baseline.c_str());
    return 1;
  }

  if (expected == digestLine) {
    std::printf("golden-run: corpus stream matches baseline\n  %s\n",
                digestLine.c_str());
    return 0;
  }

  std::fprintf(stderr,
               "golden-run: STREAM DIVERGES FROM BASELINE\n"
               "  expected: %s\n"
               "  actual:   %s\n"
               "if this change was intentional (or the baseline is still "
               "in the retired file-tree format), delete %s and rerun to "
               "re-pin\n",
               expected.c_str(), digestLine.c_str(), baseline.c_str());
  return 1;
}
