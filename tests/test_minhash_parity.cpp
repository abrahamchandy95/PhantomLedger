#include "phantomledger/exporter/aml/minhash.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <string_view>

namespace mh = ::PhantomLedger::exporter::aml::minhash;

namespace {

int failures = 0;

void check(bool condition, const char *what) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
  }
}

void checkEqU32(std::uint32_t got, std::uint32_t want, const char *what) {
  if (got != want) {
    std::fprintf(stderr, "FAIL: %s — got %u, want %u\n", what, got, want);
    ++failures;
  }
}

void checkEqStr(const std::string &got, const char *want, const char *what) {
  if (got != want) {
    std::fprintf(stderr, "FAIL: %s — got '%s', want '%s'\n", what, got.c_str(),
                 want);
    ++failures;
  }
}

std::span<const std::uint8_t> bytesOf(std::string_view s) {
  return {reinterpret_cast<const std::uint8_t *>(s.data()), s.size()};
}

} // namespace

int main() {
  // ────────── MurmurHash2 known-good vectors ──────────
  checkEqU32(mh::murmurhash2(bytesOf("abc")), 324500635U, "murmurhash2('abc')");
  checkEqU32(mh::murmurhash2(bytesOf("John Smith")), 2999533813U,
             "murmurhash2('John Smith')");
  checkEqU32(mh::murmurhash2(bytesOf("")), 0U, "murmurhash2('')");

  // ────────── MinHash signature against Python reference ──────────
  {
    const auto sig = mh::signature("john smith");
    // First four band signatures, computed against the Python ref.
    checkEqU32(sig[0], 155955233U, "signature('john smith')[0]");
    checkEqU32(sig[1], 53916894U, "signature('john smith')[1]");
    checkEqU32(sig[2], 563570025U, "signature('john smith')[2]");
    checkEqU32(sig[3], 744779157U, "signature('john smith')[3]");
    // Last band (b=10 → idx 9).
    checkEqU32(sig[9], 98432971U, "signature('john smith')[9]");
  }

  // ────────── Bucket ID format ──────────
  {
    const auto ids = mh::nameMinhashIds("John", "Smith");
    check(ids.size() == 10, "nameMinhashIds emits 10 bucket IDs");
    check(!ids.empty() && ids.front().rfind("NMH_0_", 0) == 0,
          "nameMinhashIds[0] starts with 'NMH_0_'");
    check(!ids.empty() && ids.back().rfind("NMH_9_", 0) == 0,
          "nameMinhashIds[9] starts with 'NMH_9_'");
  }
  checkEqStr(mh::cityMinhashId("San Francisco"), "CMH_san_francisco",
             "cityMinhashId('San Francisco')");
  checkEqStr(mh::cityMinhashId("  NEW YORK  "), "CMH_new_york",
             "cityMinhashId(' NEW YORK ') strips + lowers + underscores");
  checkEqStr(mh::stateMinhashId("ca"), "STMH_CA", "stateMinhashId('ca')");
  checkEqStr(mh::stateMinhashId(" ca "), "STMH_CA",
             "stateMinhashId(' ca ') strips");

  // ────────── Determinism: same input → same output ──────────
  {
    const auto a = mh::nameMinhashIds("Jane", "Doe");
    const auto b = mh::nameMinhashIds("Jane", "Doe");
    check(a == b, "nameMinhashIds is deterministic");
  }

  // ────────── Differentiation: different input → different output ──────────
  {
    const auto a = mh::signature("john smith");
    const auto b = mh::signature("jane doe");
    check(a != b, "different inputs produce different signatures");
  }

  if (failures > 0) {
    std::fprintf(stderr, "\n%d test(s) failed\n", failures);
    return 1;
  }
  std::printf("All MinHash parity checks passed.\n");
  return 0;
}
