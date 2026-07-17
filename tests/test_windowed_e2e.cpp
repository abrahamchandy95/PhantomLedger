//
// tests/test_windowed_e2e.cpp
//
// End-to-end CLI equivalence for the windowed engine, per use case, two
// layers each:
//
//   1. STREAM: the binary, run with identical seed/population/window,
//      must print the SAME "Stream digest: <hex>  rows: <N>" line under
//      PL_ENGINE=monolithic and under the DEFAULT engine selection.
//
//   2. FILES: BOTH engines must produce the identical CSV file set —
//      same relative paths, byte-identical contents (BLAKE2b per file).
//
// The windowed legs deliberately set NO engine override: they exercise
// the automatic selection, so this gate ALSO pins that the default
// engine is windowed for every use case it covers (asserted via the
// self-describing "Engine: ..." line). There is no --engine flag; the
// monolithic reference engine is test infrastructure, reachable only
// through the PL_ENGINE=monolithic environment override used by the
// reference legs.
//
// Covered use cases:
//
//   standard  (--show-transactions): entity tables via exportEntities +
//             has_paid / account_flow_agg / transactions.csv via the
//             streaming exporter vs the corpus-based exportAll.
//   mule-ml:  party / transfer / account_device / account_ip via
//             StreamingMuleMlExport vs the corpus-based exportAll.
//   aml:      ~50 vertex/edge tables; the transaction-streamed tables
//             via StreamingAmlExport, the remainder via
//             exportFromArtifacts with the posted-book handoff. The
//             corpus exportAll runs the SAME sink, so this leg holds
//             the shared code path honest across engines.
//
// Any drift between a streaming accumulator and its corpus-based twin —
// including a single rounding or ordering difference — fails hard with
// the differing filenames listed.
//
// This gate is serverless: PL_FILE_ONLY=1 is the sanctioned harness
// escape from the fail-fast PostgreSQL requirement, and PL_PG
// additionally pins an unreachable target as defense in depth so a
// broken escape can never touch a developer's mirror (same convention
// as test_run_golden).
//
// HARD-ENFORCED.
//

#include "phantomledger/primitives/crypto/blake2b.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifndef PL_BIN_PATH
#error "PL_BIN_PATH must be defined (path to the phantomledger binary)"
#endif

namespace fs = std::filesystem;
using PhantomLedger::crypto::blake2b::Stream;

namespace {

constexpr const char *kDigestPrefix = "Stream digest: ";
constexpr const char *kEnginePrefix = "Engine: ";
constexpr std::size_t kDigestBytes = 32;

std::string hashFile(const fs::path &p) {
  Stream hasher{kDigestBytes};
  std::ifstream in{p, std::ios::binary};
  std::vector<char> buf(64 * 1024);
  while (in.read(buf.data(), static_cast<std::streamsize>(buf.size())) ||
         in.gcount() > 0) {
    if (!hasher.update(buf.data(), static_cast<std::size_t>(in.gcount()))) {
      std::fprintf(stderr, "hash update failed for %s\n", p.c_str());
      std::exit(1);
    }
  }
  std::uint8_t out[kDigestBytes];
  if (!hasher.finalize(out, sizeof(out))) {
    std::fprintf(stderr, "hash finalize failed for %s\n", p.c_str());
    std::exit(1);
  }
  static constexpr char kHex[] = "0123456789abcdef";
  std::string hex;
  hex.reserve(kDigestBytes * 2);
  for (const auto b : out) {
    hex.push_back(kHex[b >> 4U]);
    hex.push_back(kHex[b & 0x0FU]);
  }
  return hex;
}

// "<hash>  ./<relative path>" for every regular file, sorted by path.
[[nodiscard]] std::vector<std::string> hashTree(const fs::path &dir) {
  std::vector<std::string> lines;
  for (const auto &entry : fs::recursive_directory_iterator(dir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const auto rel = fs::relative(entry.path(), dir).generic_string();
    lines.push_back(hashFile(entry.path()) + "  ./" + rel);
  }
  std::sort(lines.begin(), lines.end(),
            [](const std::string &a, const std::string &b) {
              return a.substr(kDigestBytes * 2) < b.substr(kDigestBytes * 2);
            });
  return lines;
}

struct LegOutput {
  std::string streamLine;
  std::string engineLine;
  fs::path outDir;
};

// Runs the binary with the shared deterministic config plus a per-leg
// environment prefix (e.g. "PL_ENGINE=monolithic ") and asserts the
// self-reported engine matches `expectedEngine` — the no-override legs
// thereby pin the automatic default.
[[nodiscard]] LegOutput runLeg(const std::string &label,
                               const std::string &usecase,
                               const std::string &engineEnv,
                               const std::string &expectedEngine) {
  const fs::path outDir =
      fs::temp_directory_path() / ("pl_windowed_e2e_" + label);
  const fs::path logPath =
      fs::temp_directory_path() / ("pl_windowed_e2e_" + label + ".log");
  fs::remove_all(outDir);

  const std::string cmd =
      std::string{"PL_FILE_ONLY=1 PL_PG='host=127.0.0.1 port=9 "
                  "dbname=pl_disabled' "} +
      engineEnv + "\"" + PL_BIN_PATH + "\" --usecase " + usecase +
      " --population 1000 --days 60"
      " --seed 20260723 --show-transactions --out \"" +
      outDir.string() + "\" > \"" + logPath.string() + "\" 2>&1";

  std::printf("  running %s leg ...\n", label.c_str());
  std::fflush(stdout);

  if (const int rc = std::system(cmd.c_str()); rc != 0) {
    std::fprintf(stderr, "binary (%s) exited %d; log: %s\n", label.c_str(), rc,
                 logPath.c_str());
    std::exit(1);
  }

  LegOutput out;
  out.outDir = outDir;

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

// Returns true when the use case's two engines agree on stream and files.
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

  const auto monoFiles = hashTree(mono.outDir);
  const auto windowedFiles = hashTree(windowed.outDir);

  if (monoFiles.empty()) {
    std::fprintf(stderr, "%s: monolithic leg produced no output files\n",
                 usecase.c_str());
    return false;
  }

  if (monoFiles == windowedFiles) {
    std::printf("  PASS: identical stream and %zu byte-identical CSVs\n",
                monoFiles.size());
    std::fflush(stdout);
    return true;
  }

  std::fprintf(stderr, "WINDOWED CLI DIVERGES (%s, CSVs):\n", usecase.c_str());
  std::size_t shown = 0;
  for (const auto &line : windowedFiles) {
    if (std::find(monoFiles.begin(), monoFiles.end(), line) ==
            monoFiles.end() &&
        shown < 10) {
      std::fprintf(stderr, "  windowed-only-or-changed: %s\n",
                   line.substr(kDigestBytes * 2 + 2).c_str());
      ++shown;
    }
  }
  for (const auto &line : monoFiles) {
    if (std::find(windowedFiles.begin(), windowedFiles.end(), line) ==
            windowedFiles.end() &&
        shown < 10) {
      std::fprintf(stderr, "  monolithic-only-or-changed: %s\n",
                   line.substr(kDigestBytes * 2 + 2).c_str());
      ++shown;
    }
  }
  return false;
}

} // namespace

int main() {
  std::printf("=== Windowed CLI end-to-end: stream + export parity ===\n");

  bool ok = true;
  ok = checkUseCase("standard") && ok;
  ok = checkUseCase("mule-ml") && ok;
  ok = checkUseCase("aml") && ok;

  if (!ok) {
    std::fprintf(stderr, "WINDOWED CLI HARD FAILURE (see above)\n");
    return 1;
  }

  std::printf("WINDOWED CLI HOLDS: standard, mule-ml and aml are "
              "byte-identical between engines, with windowed as the "
              "default.\n");
  return 0;
}
