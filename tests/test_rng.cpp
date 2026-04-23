#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/probability/distributions/cdf.hpp"

#include "test_support.hpp"

#include <cstdio>
#include <set>
#include <vector>

using namespace PhantomLedger;

namespace {

void testFromSeedDeterministic() {
  auto a = random::Rng::fromSeed(42);
  auto b = random::Rng::fromSeed(42);

  for (int i = 0; i < 100; ++i) {
    PL_CHECK_EQ(a.nextU64(), b.nextU64());
  }
  std::printf("  PASS: fromSeed is deterministic\n");
}

void testDifferentSeedsDiverge() {
  auto a = random::Rng::fromSeed(42);
  auto b = random::Rng::fromSeed(43);

  int same = 0;
  for (int i = 0; i < 100; ++i) {
    if (a.nextU64() == b.nextU64())
      ++same;
  }
  PL_CHECK(same < 5);
  std::printf("  PASS: different seeds diverge\n");
}

void testCoinEdgeCases() {
  auto rng = random::Rng::fromSeed(99);
  PL_CHECK(!rng.coin(0.0));
  PL_CHECK(!rng.coin(-1.0));
  PL_CHECK(rng.coin(1.0));
  PL_CHECK(rng.coin(2.0));
  std::printf("  PASS: coin edge cases\n");
}

void testCoinDistribution() {
  auto rng = random::Rng::fromSeed(100);
  int heads = 0;
  const int N = 100000;
  for (int i = 0; i < N; ++i) {
    if (rng.coin(0.3))
      ++heads;
  }
  double ratio = static_cast<double>(heads) / static_cast<double>(N);
  PL_CHECK(ratio > 0.28 && ratio < 0.32);
  std::printf("  PASS: coin(0.3) produces ~30%% heads (got %.4f)\n", ratio);
}

void testUniformInt() {
  auto rng = random::Rng::fromSeed(200);
  for (int i = 0; i < 10000; ++i) {
    auto val = rng.uniformInt(5, 10);
    PL_CHECK(val >= 5 && val < 10);
  }
  PL_CHECK_THROWS(rng.uniformInt(10, 10));
  PL_CHECK_THROWS(rng.uniformInt(11, 10));
  std::printf("  PASS: uniformInt bounds\n");
}

void testNextDoubleRange() {
  auto rng = random::Rng::fromSeed(300);
  for (int i = 0; i < 100000; ++i) {
    double d = rng.nextDouble();
    PL_CHECK(d >= 0.0 && d < 1.0);
  }
  std::printf("  PASS: nextDouble in [0, 1)\n");
}

void testChoiceIndex() {
  auto rng = random::Rng::fromSeed(400);
  for (int i = 0; i < 10000; ++i) {
    auto idx = rng.choiceIndex(7);
    PL_CHECK(idx < 7);
  }
  PL_CHECK_THROWS(rng.choiceIndex(0));
  std::printf("  PASS: choiceIndex\n");
}

void testChoiceIndicesWithoutReplacement() {
  auto rng = random::Rng::fromSeed(500);
  auto indices = rng.choiceIndices(20, 10, false);
  PL_CHECK_EQ(indices.size(), 10U);

  // All unique
  std::set<std::size_t> unique(indices.begin(), indices.end());
  PL_CHECK_EQ(unique.size(), 10U);

  // All in range
  for (auto idx : indices) {
    PL_CHECK(idx < 20);
  }
  std::printf("  PASS: choiceIndices without replacement\n");
}

void testChoiceIndicesWithReplacement() {
  auto rng = random::Rng::fromSeed(600);
  auto indices = rng.choiceIndices(5, 20, true);
  PL_CHECK_EQ(indices.size(), 20U);
  for (auto idx : indices) {
    PL_CHECK(idx < 5);
  }
  std::printf("  PASS: choiceIndices with replacement\n");
}

void testChoiceIndicesErrors() {
  auto rng = random::Rng::fromSeed(700);

  // Empty result for k=0
  auto empty = rng.choiceIndices(10, 0, false);
  PL_CHECK(empty.empty());

  // n=0 with k>0 throws
  PL_CHECK_THROWS(rng.choiceIndices(0, 1, false));

  // k > n without replacement throws
  PL_CHECK_THROWS(rng.choiceIndices(3, 5, false));

  std::printf("  PASS: choiceIndices error cases\n");
}

void testWeightedIndex() {
  auto rng = random::Rng::fromSeed(800);
  std::vector<double> cdf = {0.1, 0.3, 0.6, 1.0};
  int counts[4] = {};
  const int N = 100000;
  for (int i = 0; i < N; ++i) {
    auto idx = distributions::sampleIndex(cdf, rng.nextDouble());
    PL_CHECK(idx < 4);
    ++counts[idx];
  }

  // Check rough proportions: 10%, 20%, 30%, 40%
  double r0 = static_cast<double>(counts[0]) / N;
  double r1 = static_cast<double>(counts[1]) / N;
  double r2 = static_cast<double>(counts[2]) / N;
  double r3 = static_cast<double>(counts[3]) / N;

  PL_CHECK(r0 > 0.08 && r0 < 0.12);
  PL_CHECK(r1 > 0.18 && r1 < 0.22);
  PL_CHECK(r2 > 0.28 && r2 < 0.32);
  PL_CHECK(r3 > 0.38 && r3 < 0.42);

  std::printf("  PASS: weightedIndex distribution (%.2f, %.2f, %.2f, %.2f)\n",
              r0, r1, r2, r3);
}

void testFactoryDeterministic() {
  random::RngFactory f(7);
  auto a = f.rng({"people"});
  auto b = f.rng({"people"});

  PL_CHECK_EQ(a.nextU64(), b.nextU64());

  auto c = f.rng({"accounts"});
  // Different namespace -> different stream
  auto a2 = f.rng({"people"});
  PL_CHECK(c.nextU64() != a2.nextU64());

  std::printf("  PASS: RngFactory deterministic and independent\n");
}

} // namespace

int main() {
  std::printf("=== Rng Tests ===\n");
  testFromSeedDeterministic();
  testDifferentSeedsDiverge();
  testCoinEdgeCases();
  testCoinDistribution();
  testUniformInt();
  testNextDoubleRange();
  testChoiceIndex();
  testChoiceIndicesWithoutReplacement();
  testChoiceIndicesWithReplacement();
  testChoiceIndicesErrors();
  testWeightedIndex();
  testFactoryDeterministic();
  std::printf("All Rng tests passed.\n\n");
  return 0;
}
