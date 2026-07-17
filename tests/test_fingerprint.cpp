//
// tests/test_fingerprint.cpp
//
// Unit tests for the acceptance fingerprint diagnostics
// (pipeline/acceptance/fingerprint.hpp).
//
// The invariance gates rely on firstDifference() to name the first
// diverging field; these tests pin its contract so a diagnostics
// regression cannot silently blunt every gate that uses it:
//
//   - exact equality reports an empty string;
//   - fields are checked in declaration order (rows before digest, digest
//     before candidate count, ...);
//   - drop-map differences name the map, the key and both counts,
//     including keys present on only one side.
//

#include "phantomledger/pipeline/acceptance/fingerprint.hpp"

#include "test_support.hpp"

#include <cstdio>
#include <string>

namespace acceptance = PhantomLedger::pipeline::acceptance;

namespace {

[[nodiscard]] bool contains(const std::string &haystack, const char *needle) {
  return haystack.find(needle) != std::string::npos;
}

[[nodiscard]] acceptance::RunFingerprint baseFingerprint() {
  acceptance::RunFingerprint fp;
  fp.rows = 100;
  fp.digest = "abc";
  fp.candidateRows = 90;
  fp.fraudRows = 10;
  fp.cardEvents = 5;
  fp.preDropsByReason = {{"nsf", 3}, {"limit", 1}};
  fp.preDropsByChannel = {{{"nsf", 2u}, 3u}};
  fp.postDropsByReason = {{"nsf", 1}};
  fp.postDropsByChannel = {};
  fp.bookHash = 0xDEADBEEF;
  return fp;
}

void testEqualFingerprintsReportEmpty() {
  const auto a = baseFingerprint();
  const auto b = baseFingerprint();
  PL_CHECK(acceptance::firstDifference(a, b).empty());
  std::printf("  PASS: equal fingerprints -> empty difference\n");
}

void testFieldOrderPrecedence() {
  // rows differs AND digest differs: rows must be reported first.
  auto a = baseFingerprint();
  auto b = baseFingerprint();
  b.rows = 101;
  b.digest = "xyz";
  const auto d = acceptance::firstDifference(a, b);
  PL_CHECK(contains(d, "rows"));
  PL_CHECK(!contains(d, "digest"));
  std::printf("  PASS: rows reported before digest\n");
}

void testScalarFields() {
  {
    auto b = baseFingerprint();
    b.digest = "xyz";
    const auto d = acceptance::firstDifference(baseFingerprint(), b);
    PL_CHECK(contains(d, "digest"));
    PL_CHECK(contains(d, "abc"));
    PL_CHECK(contains(d, "xyz"));
  }
  {
    auto b = baseFingerprint();
    b.candidateRows = 91;
    const auto d = acceptance::firstDifference(baseFingerprint(), b);
    PL_CHECK(contains(d, "candidateRows"));
    PL_CHECK(contains(d, "90"));
    PL_CHECK(contains(d, "91"));
  }
  {
    auto b = baseFingerprint();
    b.fraudRows = 11;
    PL_CHECK(
        contains(acceptance::firstDifference(baseFingerprint(), b), "fraudRows"));
  }
  {
    auto b = baseFingerprint();
    b.cardEvents = 6;
    PL_CHECK(contains(acceptance::firstDifference(baseFingerprint(), b),
                      "cardEvents"));
  }
  {
    auto b = baseFingerprint();
    b.bookHash = 0xFEEDFACE;
    const auto d = acceptance::firstDifference(baseFingerprint(), b);
    PL_CHECK(contains(d, "bookHash"));
  }
  std::printf("  PASS: every scalar field is named when it differs\n");
}

void testDropMapCountMismatch() {
  auto b = baseFingerprint();
  b.preDropsByReason["nsf"] = 4;
  const auto d = acceptance::firstDifference(baseFingerprint(), b);
  PL_CHECK(contains(d, "preDropsByReason"));
  PL_CHECK(contains(d, "nsf"));
  PL_CHECK(contains(d, "3"));
  PL_CHECK(contains(d, "4"));
  std::printf("  PASS: drop-map count mismatch names map, key and counts\n");
}

void testDropMapMissingKey() {
  {
    // Key only in leg A.
    auto b = baseFingerprint();
    b.preDropsByReason.erase("limit");
    const auto d = acceptance::firstDifference(baseFingerprint(), b);
    PL_CHECK(contains(d, "preDropsByReason"));
    PL_CHECK(contains(d, "limit"));
    PL_CHECK(contains(d, "leg A"));
  }
  {
    // Key only in leg B.
    auto b = baseFingerprint();
    b.postDropsByReason.emplace("overdraft", 7u);
    const auto d = acceptance::firstDifference(baseFingerprint(), b);
    PL_CHECK(contains(d, "postDropsByReason"));
    PL_CHECK(contains(d, "overdraft"));
    PL_CHECK(contains(d, "leg B"));
  }
  std::printf("  PASS: one-sided drop-map keys name the owning leg\n");
}

void testChannelMapMismatch() {
  auto b = baseFingerprint();
  b.preDropsByChannel[{"nsf", 2u}] = 9;
  const auto d = acceptance::firstDifference(baseFingerprint(), b);
  PL_CHECK(contains(d, "preDropsByChannel"));
  PL_CHECK(contains(d, "nsf"));
  PL_CHECK(contains(d, "channel 2"));
  std::printf("  PASS: channel-map mismatch names reason and channel\n");
}

} // namespace

int main() {
  std::printf("=== RunFingerprint Diagnostics ===\n");
  testEqualFingerprintsReportEmpty();
  testFieldOrderPrecedence();
  testScalarFields();
  testDropMapCountMismatch();
  testDropMapMissingKey();
  testChannelMapMismatch();
  std::printf("all fingerprint diagnostics tests passed\n");
  return 0;
}
