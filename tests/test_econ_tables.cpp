//
// tests/test_econ_tables.cpp
//
// macro-history-v1 H0.5: the era reference tables (exporter::econ::
// writeEraTables) pinned SERVERLESSLY through the TableCapture seam —
// no PostgreSQL, no files. The contract: the rendered content of
// econ.macro_annual / econ.mortality / econ.provenance is EXACTLY the
// EMBEDDED pinned data (synth/econ/era_data.hpp — the constexpr
// tables that replaced the retired data/econ CSVs), row for row. The
// writer renders CRLF row endings, so comparison is per-line. The
// tables live in their own schema precisely so the public-schema
// table golden does not move — THIS test is their content pin.
// Expected row counts derive from the embedded arrays themselves, so
// a coverage-extension refresh (which rewrites era_data.hpp) needs no
// edit here — test_econ_catalog pins the coverage meaning.
//

#undef NDEBUG

#include "phantomledger/exporter/common/table.hpp"
#include "phantomledger/exporter/econ/export.hpp"
#include "phantomledger/synth/econ/era_data.hpp"

#include <cassert>
#include <cstdio>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pl = ::PhantomLedger;
namespace eradata = ::PhantomLedger::synth::econ::data;

namespace {

struct Capture final : pl::exporter::common::TableCapture {
  std::map<std::string, std::string> byStem;

  void put(std::string_view stem, const char *data,
           std::size_t size) override {
    byStem[std::string{stem}].append(data, size);
  }
};

// The captured table's lines: csv::Writer ends rows with CRLF.
[[nodiscard]] std::vector<std::string> capturedLines(const std::string &bytes) {
  std::vector<std::string> out;
  std::size_t pos = 0;
  while (pos < bytes.size()) {
    const std::size_t end = bytes.find("\r\n", pos);
    assert(end != std::string::npos && "every rendered row ends with CRLF");
    out.push_back(bytes.substr(pos, end - pos));
    pos = end + 2;
  }
  return out;
}

[[nodiscard]] std::string joinCells(std::span<const std::string_view> cells) {
  std::string out;
  for (std::size_t i = 0; i < cells.size(); ++i) {
    if (i > 0) {
      out += ',';
    }
    out += cells[i];
  }
  return out;
}

int g_failures = 0;

void check(bool cond, const std::string &what) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

void compareLines(const char *stem, const std::vector<std::string> &actual,
                  const std::vector<std::string> &expected) {
  check(actual.size() == expected.size(),
        std::string{stem} + ": line count " + std::to_string(actual.size()) +
            " == expected " + std::to_string(expected.size()));
  const std::size_t n = std::min(actual.size(), expected.size());
  for (std::size_t i = 0; i < n; ++i) {
    if (actual[i] != expected[i]) {
      check(false, std::string{stem} + ": line " + std::to_string(i + 1) +
                       " diverges\n  rendered: " + actual[i] +
                       "\n  embedded: " + expected[i]);
      return; // first divergence is enough
    }
  }
}

} // namespace

int main() {
  Capture cap;
  pl::exporter::econ::writeEraTables({.pg = nullptr, .capture = &cap});

  check(cap.byStem.size() == 3, "exactly three era tables rendered, got " +
                                    std::to_string(cap.byStem.size()));

  // econ.macro_annual == kMacroAnnual, cell for cell (one row per
  // covered era year).
  {
    const auto it = cap.byStem.find("macro_annual");
    check(it != cap.byStem.end(), "table captured: macro_annual");
    if (it != cap.byStem.end()) {
      std::vector<std::string> expected;
      expected.push_back(joinCells(eradata::kMacroColumns));
      for (const auto &r : eradata::kMacroAnnual) {
        expected.push_back(std::to_string(r.year) + ',' +
                           std::to_string(r.cpiUE3) + ',' +
                           std::to_string(r.awiCents) + ',' +
                           std::to_string(r.pceDollars) + ',' +
                           std::to_string(r.unempBp) + ',' +
                           std::to_string(r.recessionMonths) + ',' +
                           std::to_string(r.populationThousands));
      }
      check(expected.size() == eradata::kMacroAnnual.size() + 1,
            "macro table = header + every era year");
      compareLines("macro_annual", capturedLines(it->second), expected);
    }
  }

  // econ.mortality == kMortality (120 single ages).
  {
    const auto it = cap.byStem.find("mortality");
    check(it != cap.byStem.end(), "table captured: mortality");
    if (it != cap.byStem.end()) {
      std::vector<std::string> expected;
      expected.push_back(joinCells(eradata::kMortalityColumns));
      for (const auto &r : eradata::kMortality) {
        expected.push_back(std::to_string(r.age) + ',' +
                           std::to_string(r.qxMaleE6) + ',' +
                           std::to_string(r.qxFemaleE6));
      }
      check(expected.size() == eradata::kMortality.size() + 1,
            "mortality table = header + every age row");
      compareLines("mortality", capturedLines(it->second), expected);
    }
  }

  // econ.provenance == kSources (the refresh registry). Registry cells
  // stay comma/quote-free by convention, so verbatim joining matches
  // the writer's unquoted rendering.
  {
    const auto it = cap.byStem.find("provenance");
    check(it != cap.byStem.end(), "table captured: provenance");
    if (it != cap.byStem.end()) {
      std::vector<std::string> expected;
      expected.push_back(joinCells(eradata::kSourceColumns));
      for (const auto &r : eradata::kSources) {
        const std::string_view cells[]{
            r.seriesKey, r.target,     r.provider,      r.seriesId,
            r.access,    r.urlPrimary, r.urlFallback,   r.transform,
            r.publishedFrom, r.lastVerified, r.status};
        expected.push_back(joinCells(cells));
      }
      check(expected.size() == eradata::kSources.size() + 1,
            "provenance table = header + every registry row");
      compareLines("provenance", capturedLines(it->second), expected);
    }
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("test_econ_tables: all checks passed (3 tables, content == "
              "embedded era data)\n");
  return 0;
}
