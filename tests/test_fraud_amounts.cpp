#include "phantomledger/transfers/fraud/typologies/amounts.hpp"

#include "phantomledger/primitives/random/rng.hpp"

#include "test_support.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using namespace PhantomLedger;
namespace amounts = transfers::fraud::typologies::amounts;

namespace {

constexpr std::uint64_t kSeed = 0x5EEDF00DULL;
constexpr std::size_t kSamples = 20'000;

[[nodiscard]] bool isCents(double v) {
  const double scaled = v * 100.0;
  return std::abs(scaled - std::round(scaled)) < 1e-6;
}

template <class F> [[nodiscard]] std::vector<double> sample(F &&fn) {
  auto rng = random::Rng::fromSeed(kSeed);
  std::vector<double> out;
  out.reserve(kSamples);
  for (std::size_t i = 0; i < kSamples; ++i) {
    out.push_back(fn(rng));
  }
  return out;
}

[[nodiscard]] double median(std::vector<double> v) {
  std::sort(v.begin(), v.end());
  return v[v.size() / 2];
}

[[nodiscard]] double mean(const std::vector<double> &v) {
  double s = 0.0;
  for (const double x : v) {
    s += x;
  }
  return s / static_cast<double>(v.size());
}

void testDeterminism() {
  const auto a = sample(amounts::atoDrainAmount);
  const auto b = sample(amounts::atoDrainAmount);
  PL_CHECK_EQ(a.size(), b.size());
  for (std::size_t i = 0; i < a.size(); ++i) {
    PL_CHECK_EQ(a[i], b[i]);
  }
  std::printf("  PASS: same seed -> identical sequences\n");
}

void testCardTestCharge() {
  const auto v = sample(amounts::cardTestCharge);
  std::size_t anchorHits = 0;
  for (const double x : v) {
    PL_CHECK(x >= 0.50 && x <= 5.00);
    PL_CHECK(isCents(x));
    if (x == 0.50 || x == 1.00 || x == 2.00 || x == 5.00) {
      ++anchorHits;
    }
  }
  // 40% forced anchor mass plus incidental continuous hits; assert a
  // generous band around the target so per-toolchain FP noise never
  // flips the gate.
  const double anchorFrac =
      static_cast<double>(anchorHits) / static_cast<double>(v.size());
  PL_CHECK(anchorFrac > 0.30 && anchorFrac < 0.55);
  std::printf("  PASS: card-test band [$0.50,$5], cents, anchor mass %.3f\n",
              anchorFrac);
}

void testCardFraudSpend() {
  const auto v = sample(amounts::cardFraudSpend);
  for (const double x : v) {
    PL_CHECK(x >= 1.0 && x <= 5000.0);
    PL_CHECK(isCents(x));
  }
  const double med = median(v);
  const double avg = mean(v);
  PL_CHECK(med > 67.0 && med < 91.0);   // target median $79 +/- 15%
  PL_CHECK(avg > 120.0 && avg < 210.0); // analytic mean ~$162 pre-clamp
  std::printf("  PASS: card spend median %.2f mean %.2f\n", med, avg);
}

void testAtoDrain() {
  const auto v = sample(amounts::atoDrainAmount);
  std::size_t fiveFigure = 0;
  for (const double x : v) {
    PL_CHECK(x >= 10.0 && x <= 85000.0);
    PL_CHECK(isCents(x));
    if (x >= 10000.0) {
      ++fiveFigure;
    }
  }
  const double med = median(v);
  const double avg = mean(v);
  PL_CHECK(med > 153.0 && med < 207.0); // target median $180 +/- 15%
  PL_CHECK(avg > 350.0 && avg < 800.0); // analytic mean ~$554 pre-clamp
  // ~0.37% of draws exceed $10k analytically (~74 of 20k). Wide band:
  PL_CHECK(fiveFigure >= 20 && fiveFigure <= 200);
  std::printf("  PASS: ATO median %.2f mean %.2f five-figure tail %zu/%zu\n",
              med, avg, fiveFigure, v.size());
}

} // namespace

int main() {
  std::printf("test_fraud_amounts\n");
  testDeterminism();
  testCardTestCharge();
  testCardFraudSpend();
  testAtoDrain();
  std::printf("OK\n");
  return 0;
}
