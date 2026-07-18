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

} // namespace

int main() {
  const fs::path logPath = fs::temp_directory_path() / "pl_run_golden.log";

  const std::string cmd = std::string{"PL_FILE_ONLY=1 "
                                      "PL_PG='host=127.0.0.1 port=9 "
                                      "dbname=pl_disabled' \""} +
                          PL_BIN_PATH +
                          "\" --population 2000 --days 60"
                          " --seed 3405691582 > \"" +
                          logPath.string() + "\" 2>&1";
  if (const int rc = std::system(cmd.c_str()); rc != 0) {
    std::fprintf(stderr, "binary exited %d; log: %s\n", rc, logPath.c_str());
    return 1;
  }

  std::string digestLine;
  {
    std::ifstream in{logPath};
    std::string line;
    while (std::getline(in, line)) {
      if (line.rfind(kDigestPrefix, 0) == 0) {
        digestLine = line;
      }
    }
  }
  if (digestLine.empty()) {
    std::fprintf(stderr, "golden-run: no '%s' line in the run log: %s\n",
                 kDigestPrefix, logPath.c_str());
    return 1;
  }

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
