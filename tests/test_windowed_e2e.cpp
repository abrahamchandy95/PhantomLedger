//
// tests/test_windowed_e2e.cpp
//
// End-to-end CLI equivalence for the windowed engine, per use case: the
// binary, run with identical seed/population/window, must print the
// SAME "Stream digest: <hex>  rows: <N>" line under PL_ENGINE=monolithic
// and under the DEFAULT engine selection.
//
// The windowed legs deliberately set NO engine override: they exercise
// the automatic selection, so this gate ALSO pins that the default
// engine is windowed for every use case it covers (asserted via the
// self-describing "Engine: ..." line). There is no --engine flag; the
// monolithic reference engine is test infrastructure, reachable only
// through the PL_ENGINE=monolithic environment override used by the
// reference legs.
//
// Covered use cases: standard, mule-ml, aml. aml-txn-edges needs a live
// server (its windowed leg reads the streamed corpus back from
// PostgreSQL) and has its own gate, test_windowed_aml_txn_edges.
//
// COVERAGE NOTE (CSV retirement, step 5c): this gate used to also
// compare the two engines' CSV file trees. The legs are file-less now
// (no --out; nothing written). Engine-level EXPORTER parity rests on
// test_arch_equivalence at the library level, and the windowed engine's
// table bytes are pinned by the live-PostgreSQL table-digest golden
// (test_table_golden). The stream digest here covers the full corpus in
// row_seq order.
//
// This gate is serverless: PL_FILE_ONLY=1 is the sanctioned harness
// escape from the fail-fast PostgreSQL requirement, and PL_PG
// additionally pins an unreachable target as defense in depth so a
// broken escape can never touch a developer's mirror (same convention
// as test_run_golden).
//
// HARD-ENFORCED.
//

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#ifndef PL_BIN_PATH
#error "PL_BIN_PATH must be defined (path to the phantomledger binary)"
#endif

namespace fs = std::filesystem;

namespace {

constexpr const char *kDigestPrefix = "Stream digest: ";
constexpr const char *kEnginePrefix = "Engine: ";

struct LegOutput {
  std::string streamLine;
  std::string engineLine;
};

// Runs the binary with the shared deterministic config plus a per-leg
// environment prefix (e.g. "PL_ENGINE=monolithic ") and asserts the
// self-reported engine matches `expectedEngine` — the no-override legs
// thereby pin the automatic default.
[[nodiscard]] LegOutput runLeg(const std::string &label,
                               const std::string &usecase,
                               const std::string &engineEnv,
                               const std::string &expectedEngine) {
  const fs::path logPath =
      fs::temp_directory_path() / ("pl_windowed_e2e_" + label + ".log");

  const std::string cmd =
      std::string{"PL_FILE_ONLY=1 PL_PG='host=127.0.0.1 port=9 "
                  "dbname=pl_disabled' "} +
      engineEnv + "\"" + PL_BIN_PATH + "\" --usecase " + usecase +
      " --population 1000 --days 60"
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
    std::fprintf(stderr,
                 "%s leg reported engine '%s' (expected '%s'); the "
                 "default engine selection has regressed. log: %s\n",
                 label.c_str(), out.engineLine.c_str(), expectedEngine.c_str(),
                 logPath.c_str());
    std::exit(1);
  }
  return out;
}

// Returns true when the use case's two engines agree on the stream.
[[nodiscard]] bool checkUseCase(const std::string &usecase) {
  std::printf("--- use case: %s ---\n", usecase.c_str());

  const auto mono = runLeg("monolithic_" + usecase, usecase,
                           "PL_ENGINE=monolithic ", "monolithic");
  std::printf("  monolithic: %s\n", mono.streamLine.c_str());
  std::fflush(stdout);

  // No engine override: the automatic default MUST resolve to windowed.
  const auto windowed =
      runLeg("windowed_" + usecase, usecase, "", "windowed (auto)");
  std::printf("  windowed:   %s\n", windowed.streamLine.c_str());
  std::fflush(stdout);

  if (mono.streamLine != windowed.streamLine) {
    std::fprintf(stderr,
                 "WINDOWED CLI DIVERGES (%s, stream):\n  monolithic: %s\n  "
                 "windowed:   %s\n",
                 usecase.c_str(), mono.streamLine.c_str(),
                 windowed.streamLine.c_str());
    return false;
  }

  std::printf("  PASS: identical stream digest\n");
  std::fflush(stdout);
  return true;
}

} // namespace

int main() {
  std::printf("=== Windowed CLI end-to-end: stream parity ===\n");

  bool ok = true;
  ok = checkUseCase("standard") && ok;
  ok = checkUseCase("mule-ml") && ok;
  ok = checkUseCase("aml") && ok;

  if (!ok) {
    std::fprintf(stderr, "WINDOWED CLI HARD FAILURE (see above)\n");
    return 1;
  }

  std::printf("WINDOWED CLI HOLDS: standard, mule-ml and aml stream "
              "digests are identical between engines, with windowed as "
              "the default.\n");
  return 0;
}
