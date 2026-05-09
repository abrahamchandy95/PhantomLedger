#include "phantomledger/primitives/random/distributions/cdf.hpp"

#include "test_support.hpp"

#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace PhantomLedger;

namespace {

constexpr double eps = 1e-10;

bool almostEqual(double lhs, double rhs) { return std::abs(lhs - rhs) < eps; }

void testBuildCdf() {
  const std::vector<double> weights = {1.0, 2.0, 3.0, 4.0};
  const auto cdf = probability::distributions::buildCdf(weights);

  PL_CHECK_EQ(cdf.size(), 4U);
  PL_CHECK(almostEqual(cdf[0], 0.1));
  PL_CHECK(almostEqual(cdf[1], 0.3));
  PL_CHECK(almostEqual(cdf[2], 0.6));
  PL_CHECK_EQ(cdf[3], 1.0);
  std::printf("  PASS: buildCdf [1,2,3,4]\n");
}

void testBuildCdfUniform() {
  const std::vector<double> weights = {5.0, 5.0, 5.0};
  const auto cdf = probability::distributions::buildCdf(weights);

  PL_CHECK_EQ(cdf.size(), 3U);
  PL_CHECK(almostEqual(cdf[0], 1.0 / 3.0));
  PL_CHECK(almostEqual(cdf[1], 2.0 / 3.0));
  PL_CHECK_EQ(cdf[2], 1.0);
  std::printf("  PASS: buildCdf uniform weights\n");
}

void testBuildCdfSingle() {
  const auto cdf =
      probability::distributions::buildCdf(std::vector<double>{42.0});
  PL_CHECK_EQ(cdf.size(), 1U);
  PL_CHECK_EQ(cdf[0], 1.0);
  std::printf("  PASS: buildCdf single weight\n");
}

void testBuildCdfErrors() {
  bool caught = false;
  try {
    (void)probability::distributions::buildCdf(std::vector<double>{});
  } catch (const std::invalid_argument &) {
    caught = true;
  }
  PL_CHECK(caught);

  caught = false;
  try {
    (void)probability::distributions::buildCdf(std::vector<double>{1.0, -1.0});
  } catch (const std::invalid_argument &) {
    caught = true;
  }
  PL_CHECK(caught);

  caught = false;
  try {
    (void)probability::distributions::buildCdf(std::vector<double>{0.0, 0.0});
  } catch (const std::invalid_argument &) {
    caught = true;
  }
  PL_CHECK(caught);

  caught = false;
  try {
    (void)probability::distributions::buildCdf(
        std::vector<double>{1.0, std::numeric_limits<double>::infinity()});
  } catch (const std::invalid_argument &) {
    caught = true;
  }
  PL_CHECK(caught);
  std::printf("  PASS: buildCdf error handling\n");
}

void testSampleIndexValues() {
  const std::vector<double> cdf = {0.1, 0.3, 0.6, 1.0};

  struct Case {
    double u;
    std::size_t expected;
  };

  const Case cases[] = {
      {0.00, 0}, {0.05, 0}, {0.15, 1}, {0.35, 2}, {0.65, 3}, {0.99, 3},
  };

  for (const auto &c : cases) {
    PL_CHECK_EQ(probability::distributions::sampleIndex(cdf, c.u), c.expected);
  }
  std::printf("  PASS: sampleIndex matches numpy searchsorted(right)\n");
}

void testSampleIndexEdges() {
  const std::vector<double> cdf = {0.25, 0.50, 0.75, 1.0};
  PL_CHECK_EQ(probability::distributions::sampleIndex(cdf, 0.25), 1U);
  PL_CHECK_EQ(probability::distributions::sampleIndex(cdf, 0.50), 2U);
  PL_CHECK_EQ(probability::distributions::sampleIndex(cdf, 0.0), 0U);
  PL_CHECK_EQ(probability::distributions::sampleIndex(cdf, -1.0), 0U);
  PL_CHECK_EQ(probability::distributions::sampleIndex(cdf, 2.0), 3U);
  std::printf("  PASS: sampleIndex edge behavior\n");
}

void testSampleIndexErrors() {
  bool caught = false;
  try {
    (void)probability::distributions::sampleIndex(std::vector<double>{}, 0.5);
  } catch (const std::invalid_argument &) {
    caught = true;
  }
  PL_CHECK(caught);
  std::printf("  PASS: sampleIndex rejects empty CDF\n");
}

} // namespace

int main() {
  std::printf("=== Distribution Tests (buildCdf / sampleIndex) ===\n");
  testBuildCdf();
  testBuildCdfUniform();
  testBuildCdfSingle();
  testBuildCdfErrors();
  testSampleIndexValues();
  testSampleIndexEdges();
  testSampleIndexErrors();
  std::printf("All distribution tests passed.\n\n");
  return 0;
}
