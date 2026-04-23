#include "phantomledger/entropy/random/seed.hpp"

#include "test_support.hpp"

#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using namespace PhantomLedger;

namespace {

void testSinglePart() {
  const auto got = random::deriveSeed(7, {"people"});
  PL_CHECK_EQ(got, 5707781807747411345ULL);
  std::printf("  PASS: deriveSeed(7, {people})\n");
}

void testSinglePartTwo() {
  const auto got = random::deriveSeed(7, {"accounts"});
  PL_CHECK_EQ(got, 11695625671488150687ULL);
  std::printf("  PASS: deriveSeed(7, {accounts})\n");
}

void testMultiPart() {
  const auto got = random::deriveSeed(7, {"persona_individual", "C0000000001"});
  PL_CHECK_EQ(got, 12457773100242140889ULL);
  std::printf("  PASS: deriveSeed multi-part key\n");
}

void testDeterministic() {
  const auto a = random::deriveSeed(42, {"hello", "world"});
  const auto b = random::deriveSeed(42, {"hello", "world"});
  const auto c = random::deriveSeed(42, {"hello", "other"});

  PL_CHECK_EQ(a, b);
  PL_CHECK(a != c);
  std::printf("  PASS: deriveSeed is deterministic\n");
}

void testSpanOverload() {
  const std::string_view parts[] = {"persona_individual", "C0000000001"};
  const auto got =
      random::deriveSeed(7, std::span<const std::string_view>{parts});
  PL_CHECK_EQ(got, 12457773100242140889ULL);
  std::printf("  PASS: span overload matches initializer_list\n");
}

void testVectorOverload() {
  const std::vector<std::string> parts = {"persona_individual", "C0000000001"};
  const auto got = random::deriveSeed(7, parts);
  PL_CHECK_EQ(got, 12457773100242140889ULL);
  std::printf("  PASS: vector<string> overload matches initializer_list\n");
}

} // namespace

int main() {
  std::printf("=== Seed Derivation Tests ===\n");
  testSinglePart();
  testSinglePartTwo();
  testMultiPart();
  testDeterministic();
  testSpanOverload();
  testVectorOverload();
  std::printf("All seed tests passed.\n\n");
  return 0;
}
