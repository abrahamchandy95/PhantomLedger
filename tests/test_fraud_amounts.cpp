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

[[nodiscard]] bool isTens(double v) {
  const double scaled = v / 10.0;
  return std::abs(scaled - std::round(scaled)) < 1e-9;
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

// H1 step 2b: the continuous samplers take the event year's CPI level
// multiplier (median AND clamps scale together); 1.0 is the
// calibration-year identity. Default arguments don't travel through
// function references, so the harness binds the scale explicitly.
[[nodiscard]] auto atoAt(double scale) {
  return [scale](random::Rng &rng) {
    return amounts::atoDrainAmount(rng, scale);
  };
}

[[nodiscard]] auto cardSpendAt(double scale) {
  return [scale](random::Rng &rng) {
    return amounts::cardFraudSpend(rng, scale);
  };
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
  const auto a = sample(atoAt(1.0));
  const auto b = sample(atoAt(1.0));
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
  const auto v = sample(cardSpendAt(1.0));
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
  const auto v = sample(atoAt(1.0));
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

// H1 step 2b (class F): a backward era scale moves the whole
// distribution — median and BOTH clamps — proportionally, so the tail
// shape is era-invariant. The denomination samplers (cardTestCharge,
// giftCardScamAmount) take no scale by design (fixed-nominal
// lattices, authority U-6) — their gates above already pin that.
// Scaled clamp bounds land on sub-cent values and the sampler rounds
// to CENTS, so a low-clamped draw can sit up to half a cent below
// `bound × s` — the band checks carry a one-cent tolerance.
void testEraScaling() {
  const double s = 0.533; // ~priceScale(1991)

  const auto card = sample(cardSpendAt(s));
  for (const double x : card) {
    PL_CHECK(x >= 1.0 * s - 0.01 && x <= 5000.0 * s + 0.01);
    PL_CHECK(isCents(x));
  }
  const double cardMed = median(card);
  PL_CHECK(cardMed > 67.0 * s && cardMed < 91.0 * s);

  const auto ato = sample(atoAt(s));
  for (const double x : ato) {
    PL_CHECK(x >= 10.0 * s - 0.01 && x <= 85000.0 * s + 0.01);
    PL_CHECK(isCents(x));
  }
  const double atoMed = median(ato);
  PL_CHECK(atoMed > 153.0 * s && atoMed < 207.0 * s);

  std::printf("  PASS: era scale %.3f moves medians/clamps together "
              "(card med %.2f, ATO med %.2f)\n",
              s, cardMed, atoMed);
}

void testGiftCardScamAmount() {
  const auto v = sample(amounts::giftCardScamAmount);
  std::size_t denomHits = 0;
  std::size_t maxDenomHits = 0;
  for (const double x : v) {
    PL_CHECK(x >= 50.0 && x <= 500.0); // retail rack band
    PL_CHECK(isTens(x));               // cards sell in $10 steps
    if (x == 100.0 || x == 200.0 || x == 500.0) {
      ++denomHits;
    }
    if (x == 500.0) {
      ++maxDenomHits;
    }
  }
  // RE-DERIVED, NOT WIDENED. The old band was denomFrac in (0.65, 0.90),
  // and its lower half was measured against a construction that no longer
  // exists: the sampler used to have an OFF-LATTICE branch returning
  // round(U[50,500]/10)*10, so ~25% of amounts landed on a non-denomination
  // and the complement of denomFrac was those "incidental continuous hits".
  //
  // That branch was a LEAK and was removed. It put fraud on 46 round-ten
  // values that cent-rounded legitimate spend essentially never reaches:
  // measured at pop 900 x 1461d, `amount == $310.00` was 4 fraud of 8 rows,
  // precision 0.5000 at 486x lift, a residual oracle at a value the retail
  // ladder does not even contain. Both branches now land on the ladder that
  // legitimate gift-card buyers also use (commerce/gift_cards.hpp); only the
  // WEIGHTS differ, which is the realistic relationship and the whole signal.
  //
  // So the statistic's complement changed meaning, and the check is now
  // STRONGER rather than looser: every amount must be ON the ladder, and
  // denomFrac measures concentration on the HIGH rungs against $50.
  // Derived: 0.75 * 0.97 + 0.25 * 0.80 = 0.9275.
  for (const double x : v) {
    PL_CHECK(x == 50.0 || x == 100.0 || x == 200.0 || x == 500.0);
  }
  const double denomFrac =
      static_cast<double>(denomHits) / static_cast<double>(v.size());
  const double maxFrac =
      static_cast<double>(maxDenomHits) / static_cast<double>(v.size());
  PL_CHECK(denomFrac > 0.88 && denomFrac < 0.97);
  PL_CHECK(maxFrac > 0.35 && maxFrac < 0.60); // .75 x .6 = .45 target
  const double avg = mean(v);
  const double med = median(v);
  PL_CHECK(avg > 300.0 && avg < 380.0); // analytic mean ~$339
  PL_CHECK(med > 250.0 && med <= 500.0);
  std::printf("  PASS: gift-card band [$50,$500], $10 lattice, denom mass "
              "%.3f (max-denom %.3f), median %.2f mean %.2f\n",
              denomFrac, maxFrac, med, avg);
}

} // namespace

int main() {
  std::printf("test_fraud_amounts\n");
  testDeterminism();
  testCardTestCharge();
  testCardFraudSpend();
  testAtoDrain();
  testEraScaling();
  testGiftCardScamAmount();
  std::printf("OK\n");
  return 0;
}
