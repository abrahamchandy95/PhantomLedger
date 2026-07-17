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
// and requires the same "Stream digest" line plus byte-identical CSV
// trees (~50 files under aml_txn_edges/). Leg B exercises the full
// composition: StreamingAmlTxnEdgesExport during the fold, SARs from
// the accumulated fraud groups, readback::buildBundle over the
// PostgreSQL corpus store, and exportFromArtifacts through the shared
// writer seams.
//
// NOTE: both legs write what a real run writes on this server — the
// transactions table (full rewrite), pl_run_manifest/pl_run_spans rows,
// and the aml_txn_edges mirror schema (same precedent as
// test_postgres, which also rewrites the transactions table).
//
// HARD-ENFORCED where it runs.
//

#include "phantomledger/primitives/crypto/blake2b.hpp"
#include "phantomledger/primitives/postgres/connection.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
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

[[nodiscard]] LegOutput runLeg(const std::string &conninfo,
                               const std::string &label,
                               const std::string &engineEnv,
                               const std::string &expectedEngine) {
  const fs::path outDir =
      fs::temp_directory_path() / ("pl_amltxn_e2e_" + label);
  const fs::path logPath =
      fs::temp_directory_path() / ("pl_amltxn_e2e_" + label + ".log");
  fs::remove_all(outDir);

  const std::string cmd =
      std::string{"PL_PG='"} + conninfo + "' " + engineEnv + "\"" +
      PL_BIN_PATH +
      "\" --usecase aml-txn-edges --population 1000 --days 60"
      " --seed 20260723 --out \"" +
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
    std::fprintf(stderr, "%s leg reported engine '%s' (expected '%s')\n",
                 label.c_str(), out.engineLine.c_str(),
                 expectedEngine.c_str());
    std::exit(1);
  }
  return out;
}

} // namespace

int main() {
  const char *env = std::getenv("PL_TEST_PG");
  const std::string conninfo = env != nullptr ? env : "dbname=phantomledger";

  {
    std::optional<PhantomLedger::postgres::Connection> probe;
    try {
      probe.emplace(conninfo);
    } catch (const std::exception &) {
      std::printf("SKIP: no postgres reachable via '%s'\n", conninfo.c_str());
      return 77;
    }
  }

  std::printf("=== aml-txn-edges engine parity (live PostgreSQL) ===\n");
  std::fflush(stdout);

  const auto mono =
      runLeg(conninfo, "monolithic", "PL_ENGINE=monolithic ", "monolithic");
  std::printf("  monolithic: %s\n", mono.streamLine.c_str());
  std::fflush(stdout);

  // No engine override: the DEFAULT for aml-txn-edges must resolve to
  // windowed now that PostgreSQL is guaranteed by the backend policy.
  const auto windowed = runLeg(conninfo, "windowed", "", "windowed (auto)");
  std::printf("  windowed:   %s\n", windowed.streamLine.c_str());
  std::fflush(stdout);

  if (mono.streamLine != windowed.streamLine) {
    std::fprintf(stderr,
                 "AML-TXN-EDGES DIVERGES (stream):\n  monolithic: %s\n  "
                 "windowed:   %s\n",
                 mono.streamLine.c_str(), windowed.streamLine.c_str());
    return 1;
  }

  const auto monoFiles = hashTree(mono.outDir);
  const auto windowedFiles = hashTree(windowed.outDir);

  if (monoFiles.empty()) {
    std::fprintf(stderr, "monolithic leg produced no output files\n");
    return 1;
  }

  if (monoFiles == windowedFiles) {
    std::printf("AML-TXN-EDGES HOLDS: identical stream and %zu "
                "byte-identical CSVs between engines (windowed via "
                "PostgreSQL read-back, now the default).\n",
                monoFiles.size());
    return 0;
  }

  std::fprintf(stderr, "AML-TXN-EDGES DIVERGES (CSVs):\n");
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
  return 1;
}
