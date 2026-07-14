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
#ifndef PL_GOLDEN_BASELINE
#error "PL_GOLDEN_BASELINE must be defined (path to the baseline file)"
#endif

namespace fs = std::filesystem;
using PhantomLedger::crypto::blake2b::Stream;

namespace {

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

} // namespace

int main() {
  const fs::path outDir = fs::temp_directory_path() / "pl_run_golden";
  const fs::path logPath = fs::temp_directory_path() / "pl_run_golden.log";
  fs::remove_all(outDir);

  const std::string cmd = std::string{"\""} + PL_BIN_PATH +
                          "\" --population 2000 --days 60"
                          " --seed 3405691582 --show-transactions --out \"" +
                          outDir.string() + "\" > \"" + logPath.string() +
                          "\" 2>&1";
  if (const int rc = std::system(cmd.c_str()); rc != 0) {
    std::fprintf(stderr, "binary exited %d; log: %s\n", rc, logPath.c_str());
    return 1;
  }

  std::vector<std::string> lines;
  for (const auto &entry : fs::recursive_directory_iterator(outDir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const auto rel = fs::relative(entry.path(), outDir).generic_string();
    lines.push_back(hashFile(entry.path()) + "  ./" + rel);
  }
  std::sort(lines.begin(), lines.end(),
            [](const std::string &a, const std::string &b) {
              return a.substr(kDigestBytes * 2) < b.substr(kDigestBytes * 2);
            });
  if (lines.empty()) {
    std::fprintf(stderr, "no output files produced in %s\n", outDir.c_str());
    return 1;
  }

  const fs::path baseline{PL_GOLDEN_BASELINE};
  if (!fs::exists(baseline)) {
    std::ofstream out{baseline};
    for (const auto &line : lines) {
      out << line << '\n';
    }
    std::printf("golden-run: baseline captured (%zu files) at %s\n",
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
    std::printf("golden-run: %zu files byte-identical to baseline\n",
                lines.size());
    return 0;
  }

  std::fprintf(stderr, "golden-run: OUTPUT DIVERGES FROM BASELINE\n");
  std::size_t shown = 0;
  for (const auto &line : lines) {
    if (std::find(expected.begin(), expected.end(), line) == expected.end() &&
        shown < 10) {
      std::fprintf(stderr, "  changed-or-new: %s\n",
                   line.substr(kDigestBytes * 2 + 2).c_str());
      ++shown;
    }
  }
  for (const auto &line : expected) {
    if (std::find(lines.begin(), lines.end(), line) == lines.end() &&
        shown < 10) {
      std::fprintf(stderr, "  was-in-baseline: %s\n",
                   line.substr(kDigestBytes * 2 + 2).c_str());
      ++shown;
    }
  }
  std::fprintf(stderr,
               "if this change was intentional, delete %s and rerun to "
               "re-pin\n",
               baseline.c_str());
  return 1;
}
