#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/probability/distributions/beta.hpp"
#include "phantomledger/probability/distributions/binomial.hpp"
#include "phantomledger/probability/distributions/lognormal.hpp"
#include "phantomledger/probability/distributions/poisson.hpp"

#include "test_support.hpp"

#include <cmath>
#include <cstdio>

using namespace PhantomLedger;

namespace {

constexpr double eps = 1e-6;

void testLognormalByMedian() {
  auto rng = random::Rng::fromSeed(100);

  // With sigma=0, lognormal should return exactly the median.
  double exact = probability::distributions::lognormalByMedian(rng, 500.0, 0.0);
  PL_CHECK(std::abs(exact - 500.0) < eps);

  // Sample many values and check the empirical median is close.
  const int N = 50000;
  std::vector<double> samples(N);
  for (int i = 0; i < N; ++i) {
    samples[i] = probability::distributions::lognormalByMedian(rng, 100.0, 0.5);
  }
  std::sort(samples.begin(), samples.end());
  double empiricalMedian = samples[N / 2];
  PL_CHECK(empiricalMedian > 90.0 && empiricalMedian < 110.0);

  // All positive
  PL_CHECK(samples[0] > 0.0);

  // Error cases
  PL_CHECK_THROWS(
      probability::distributions::lognormalByMedian(rng, -1.0, 0.5));
  PL_CHECK_THROWS(probability::distributions::lognormalByMedian(rng, 0.0, 0.5));
  PL_CHECK_THROWS(
      probability::distributions::lognormalByMedian(rng, 100.0, -0.1));

  std::printf("  PASS: lognormalByMedian (empirical median=%.1f)\n",
              empiricalMedian);
}

void testGamma() {
  auto rng = random::Rng::fromSeed(200);

  // Gamma(shape, scale) has mean = shape * scale.
  const int N = 50000;
  double sum = 0.0;
  for (int i = 0; i < N; ++i) {
    double val = probability::distributions::gamma(rng, 2.0, 3.0);
    PL_CHECK(val > 0.0);
    sum += val;
  }
  double mean = sum / N;
  // Expected mean = 2.0 * 3.0 = 6.0
  PL_CHECK(mean > 5.5 && mean < 6.5);

  PL_CHECK_THROWS(probability::distributions::gamma(rng, 0.0, 1.0));
  PL_CHECK_THROWS(probability::distributions::gamma(rng, 1.0, 0.0));

  std::printf("  PASS: gamma (empirical mean=%.2f, expected=6.0)\n", mean);
}

void testGammaSmallShape() {
  // Shape < 1 uses the Ahrens-Dieter boost.
  auto rng = random::Rng::fromSeed(201);
  const int N = 20000;
  double sum = 0.0;
  for (int i = 0; i < N; ++i) {
    double val = probability::distributions::gamma(rng, 0.5, 2.0);
    PL_CHECK(val > 0.0);
    sum += val;
  }
  double mean = sum / N;
  // Expected mean = 0.5 * 2.0 = 1.0
  PL_CHECK(mean > 0.8 && mean < 1.2);
  std::printf("  PASS: gamma small shape (mean=%.2f, expected=1.0)\n", mean);
}

void testBeta() {
  auto rng = random::Rng::fromSeed(300);
  const int N = 50000;
  double sum = 0.0;
  for (int i = 0; i < N; ++i) {
    double val = probability::distributions::beta(rng, 2.0, 5.0);
    PL_CHECK(val >= 0.0 && val <= 1.0);
    sum += val;
  }
  double mean = sum / N;
  // Expected mean = alpha / (alpha + beta) = 2/7 ≈ 0.2857
  PL_CHECK(mean > 0.27 && mean < 0.30);

  PL_CHECK_THROWS(probability::distributions::beta(rng, 0.0, 1.0));
  PL_CHECK_THROWS(probability::distributions::beta(rng, 1.0, 0.0));

  std::printf("  PASS: beta (empirical mean=%.4f, expected=0.2857)\n", mean);
}

void testNormal() {
  auto rng = random::Rng::fromSeed(400);
  const int N = 50000;
  double sum = 0.0;
  for (int i = 0; i < N; ++i) {
    sum += probability::distributions::normal(rng, 10.0, 2.0);
  }
  double mean = sum / N;
  PL_CHECK(mean > 9.8 && mean < 10.2);
  std::printf("  PASS: normal (empirical mean=%.3f, expected=10.0)\n", mean);
}

void testPoisson() {
  auto rng = random::Rng::fromSeed(500);

  // Small lambda (Knuth path)
  {
    const int N = 50000;
    long long sum = 0;
    for (int i = 0; i < N; ++i) {
      auto val = probability::distributions::poisson(rng, 5.0);
      PL_CHECK(val >= 0);
      sum += val;
    }
    double mean = static_cast<double>(sum) / N;
    PL_CHECK(mean > 4.8 && mean < 5.2);
    std::printf("  PASS: poisson small lambda (mean=%.2f, expected=5.0)\n",
                mean);
  }

  // Large lambda (rejection path)
  {
    const int N = 50000;
    long long sum = 0;
    for (int i = 0; i < N; ++i) {
      auto val = probability::distributions::poisson(rng, 100.0);
      PL_CHECK(val >= 0);
      sum += val;
    }
    double mean = static_cast<double>(sum) / N;
    PL_CHECK(mean > 98.0 && mean < 102.0);
    std::printf("  PASS: poisson large lambda (mean=%.2f, expected=100.0)\n",
                mean);
  }

  // Lambda <= 0 returns 0
  PL_CHECK_EQ(probability::distributions::poisson(rng, 0.0), std::int64_t{0});
  PL_CHECK_EQ(probability::distributions::poisson(rng, -1.0), std::int64_t{0});

  std::printf("  PASS: poisson edge cases\n");
}

void testBinomial() {
  auto rng = random::Rng::fromSeed(600);

  // Binomial(100, 0.3) has mean = 30.
  const int N = 50000;
  long long sum = 0;
  for (int i = 0; i < N; ++i) {
    auto val = probability::distributions::binomial(rng, 100, 0.3);
    PL_CHECK(val >= 0 && val <= 100);
    sum += val;
  }
  double mean = static_cast<double>(sum) / N;
  PL_CHECK(mean > 29.0 && mean < 31.0);

  // Edge cases
  PL_CHECK_EQ(probability::distributions::binomial(rng, 0, 0.5),
              std::int64_t{0});
  PL_CHECK_EQ(probability::distributions::binomial(rng, 10, 0.0),
              std::int64_t{0});
  PL_CHECK_EQ(probability::distributions::binomial(rng, 10, 1.0),
              std::int64_t{10});

  std::printf("  PASS: binomial (mean=%.2f, expected=30.0)\n", mean);
}

void testRoundMoney() {
  PL_CHECK(std::abs(primitives::utils::roundMoney(1.005) - 1.0) < 0.011);
  PL_CHECK(std::abs(primitives::utils::roundMoney(99.999) - 100.0) < eps);
  PL_CHECK(std::abs(primitives::utils::roundMoney(0.0) - 0.0) < eps);
  PL_CHECK(std::abs(primitives::utils::roundMoney(-1.555) - (-1.56)) < 0.011);
  std::printf("  PASS: roundMoney\n");
}

void testClamp() {
  PL_CHECK(std::abs(primitives::utils::clamp(5.0, 0.0, 10.0) - 5.0) < eps);
  PL_CHECK(std::abs(primitives::utils::clamp(-1.0, 0.0, 10.0) - 0.0) < eps);
  PL_CHECK(std::abs(primitives::utils::clamp(15.0, 0.0, 10.0) - 10.0) < eps);
  std::printf("  PASS: clamp\n");
}

void testFloorAndRound() {
  PL_CHECK(std::abs(primitives::utils::floorAndRound(5.0, 10.0) - 10.0) < eps);
  PL_CHECK(std::abs(primitives::utils::floorAndRound(15.123, 10.0) - 15.12) <
           0.011);
  std::printf("  PASS: floorAndRound\n");
}

} // namespace

int main() {
  std::printf("=== Math Sampling Tests ===\n");
  testLognormalByMedian();
  testGamma();
  testGammaSmallShape();
  testBeta();
  testNormal();
  testPoisson();
  testBinomial();
  testRoundMoney();
  testClamp();
  testFloorAndRound();
  std::printf("All math sampling tests passed.\n\n");
  return 0;
}
