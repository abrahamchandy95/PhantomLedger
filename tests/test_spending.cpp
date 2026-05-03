#include "phantomledger/spending/liquidity/factor.hpp"
#include "phantomledger/spending/liquidity/multiplier.hpp"
#include "phantomledger/spending/liquidity/snapshot.hpp"
#include "phantomledger/spending/routing/channel.hpp"
#include "phantomledger/spending/spenders/targets.hpp"

#include "test_support.hpp"

#include <cmath>
#include <cstdio>

using namespace PhantomLedger;
namespace routing = PhantomLedger::spending::routing;
namespace liquidity = PhantomLedger::spending::liquidity;
namespace spenders = PhantomLedger::spending::spenders;

namespace {

constexpr double kEps = 1e-9;

[[nodiscard]] inline bool nearly(double lhs, double rhs, double tol = kEps) {
  return std::abs(lhs - rhs) <= tol;
}

// -------------------------- Channel CDF --------------------------

void testChannelCdfShape() {
  // Equal weights for the three core channels, no unknown outflow:
  // the CDF should be (1/3, 2/3, 1, 1).
  const auto cdf = routing::buildChannelCdf(1.0, 1.0, 1.0, 0.0);
  PL_CHECK(nearly(cdf[0], 1.0 / 3.0));
  PL_CHECK(nearly(cdf[1], 2.0 / 3.0));
  PL_CHECK(nearly(cdf[2], 1.0));
  PL_CHECK(nearly(cdf[3], 1.0));
  std::printf("  PASS: channel CDF — equal weights\n");
}

void testChannelCdfWithUnknown() {
  // 20% unknown takes a fixed slice; the other 80% is split per the
  // (merchant, bills, p2p) ratio (here 6:3:1).
  const auto cdf = routing::buildChannelCdf(0.6, 0.3, 0.1, 0.20);
  // s0 = 0.8 * 0.6 = 0.48
  PL_CHECK(nearly(cdf[0], 0.48));
  // cdf[1] = 0.48 + 0.8*0.3 = 0.72
  PL_CHECK(nearly(cdf[1], 0.72));
  // cdf[2] = 0.72 + 0.8*0.1 = 0.80
  PL_CHECK(nearly(cdf[2], 0.80));
  // cdf[3] = 0.80 + 0.20 = 1.0
  PL_CHECK(nearly(cdf[3], 1.0));
  std::printf("  PASS: channel CDF — unknown carved out cleanly\n");
}

void testChannelCdfFallsBackOnZeroCore() {
  // Every core weight zero: the implementation falls back to uniform
  // 1/3 across the three core channels and respects the unknown share.
  const auto cdf = routing::buildChannelCdf(0.0, 0.0, 0.0, 0.10);
  PL_CHECK(nearly(cdf[0], 0.30));
  PL_CHECK(nearly(cdf[1], 0.60));
  PL_CHECK(nearly(cdf[2], 0.90));
  PL_CHECK(nearly(cdf[3], 1.0));
  std::printf("  PASS: channel CDF — zero core uses uniform fallback\n");
}

void testChannelCdfClampsUnknown() {
  // unknownOutflowP is clamped to [0, 1].
  const auto neg = routing::buildChannelCdf(1.0, 1.0, 1.0, -0.5);
  PL_CHECK(nearly(neg[3], 1.0));
  PL_CHECK(nearly(neg[0], 1.0 / 3.0));

  const auto over = routing::buildChannelCdf(1.0, 1.0, 1.0, 1.7);
  // All mass goes to unknown.
  PL_CHECK(nearly(over[0], 0.0));
  PL_CHECK(nearly(over[1], 0.0));
  PL_CHECK(nearly(over[2], 0.0));
  PL_CHECK(nearly(over[3], 1.0));
  std::printf("  PASS: channel CDF — unknownP clamped to [0, 1]\n");
}

void testPickSlot() {
  routing::ChannelCdf cdf{};
  cdf[0] = 0.25;
  cdf[1] = 0.50;
  cdf[2] = 0.75;
  cdf[3] = 1.00;

  PL_CHECK(routing::pickSlot(cdf, 0.0) == routing::Slot::merchant);
  PL_CHECK(routing::pickSlot(cdf, 0.10) == routing::Slot::merchant);
  PL_CHECK(routing::pickSlot(cdf, 0.30) == routing::Slot::bill);
  PL_CHECK(routing::pickSlot(cdf, 0.55) == routing::Slot::p2p);
  PL_CHECK(routing::pickSlot(cdf, 0.80) == routing::Slot::externalUnknown);
  PL_CHECK(routing::pickSlot(cdf, 0.999) == routing::Slot::externalUnknown);
  std::printf("  PASS: pickSlot ladder\n");
}

// ------------------------ Liquidity factor ------------------------

void testCountFactorMonotonic() {
  // Shape claim from the implementation: clamp to [0, 1.25], soft
  // shape below 1.0, square. So the function is monotone non-decreasing
  // in liquidity for liquidity in [0, 1.25].
  double prev = liquidity::countFactor(0.0);
  for (double x = 0.05; x <= 1.25; x += 0.05) {
    double cur = liquidity::countFactor(x);
    PL_CHECK(cur + 1e-12 >= prev);
    prev = cur;
  }
  std::printf("  PASS: countFactor monotone non-decreasing\n");
}

void testCountFactorBoundaries() {
  // At liquidity = 0:   softened = 0.50, factor = 0.25.
  PL_CHECK(nearly(liquidity::countFactor(0.0), 0.25));
  // At liquidity = 1:   softened = 1.0,  factor = 1.0.
  PL_CHECK(nearly(liquidity::countFactor(1.0), 1.0));
  // At liquidity > 1.25: clamped to 1.25, factor = 1.5625.
  PL_CHECK(nearly(liquidity::countFactor(1.25), 1.5625));
  PL_CHECK(nearly(liquidity::countFactor(5.0), 1.5625));
  // Negative liquidity is clamped to 0, then softened+squared = 0.25.
  PL_CHECK(nearly(liquidity::countFactor(-1.0), 0.25));
  std::printf("  PASS: countFactor boundary values\n");
}

// ----------------------- Liquidity multiplier ---------------------

[[nodiscard]] liquidity::Snapshot makeSnap(std::uint32_t daysSincePayday,
                                           double sensitivity,
                                           double availableCash,
                                           double baselineCash, double burden) {
  return liquidity::Snapshot{
      .daysSincePayday = daysSincePayday,
      .paycheckSensitivity = sensitivity,
      .availableCash = availableCash,
      .baselineCash = baselineCash,
      .fixedMonthlyBurden = burden,
  };
}

void testMultiplierDisabled() {
  liquidity::Throttle disabled{};
  disabled.enabled = false;

  const auto snap = makeSnap(/*daysSincePayday=*/365, /*sensitivity=*/0.9,
                             /*availableCash=*/0.0, /*baselineCash=*/1000.0,
                             /*burden=*/2000.0);
  PL_CHECK(nearly(liquidity::multiplier(disabled, snap), 1.0));
  std::printf("  PASS: liquidity multiplier — disabled returns 1.0\n");
}

void testMultiplierStressRegion() {
  const liquidity::Throttle cfg{};

  const auto fresh = makeSnap(/*daysSincePayday=*/0, /*sensitivity=*/0.5,
                              /*availableCash=*/1000.0, /*baselineCash=*/1000.0,
                              /*burden=*/0.0);

  const auto stressed =
      makeSnap(/*daysSincePayday=*/30, /*sensitivity=*/0.5,
               /*availableCash=*/1000.0, /*baselineCash=*/1000.0,
               /*burden=*/0.0);

  const double freshMult = liquidity::multiplier(cfg, fresh);
  const double stressedMult = liquidity::multiplier(cfg, stressed);

  PL_CHECK(stressedMult < freshMult);

  PL_CHECK(stressedMult >= cfg.absoluteFloor - 1e-12);
  PL_CHECK(freshMult <= liquidity::kCeiling + 1e-12);

  std::printf("  PASS: liquidity multiplier — stress region < fresh region "
              "(%.3f < %.3f)\n",
              stressedMult, freshMult);
}

void testMultiplierBurdenPenalty() {
  const liquidity::Throttle cfg{};

  const auto noBurden =
      makeSnap(/*daysSincePayday=*/4, /*sensitivity=*/0.3,
               /*availableCash=*/500.0, /*baselineCash=*/500.0,
               /*burden=*/0.0);

  const auto highBurden =
      makeSnap(/*daysSincePayday=*/4, /*sensitivity=*/0.3,
               /*availableCash=*/500.0, /*baselineCash=*/500.0,
               /*burden=*/1000.0);

  PL_CHECK(liquidity::multiplier(cfg, highBurden) <
           liquidity::multiplier(cfg, noBurden));
  std::printf("  PASS: liquidity multiplier — fixed burden penalises\n");
}

// ----------------------- Spenders helpers -------------------------

void testTotalTargetTxns() {
  // Spec: txnsPerMonth * activeSpenders * (days / 30).
  PL_CHECK(nearly(spenders::totalTargetTxns(60.0, 100, 30), 6000.0));
  PL_CHECK(nearly(spenders::totalTargetTxns(60.0, 100, 90), 18000.0));
  PL_CHECK(nearly(spenders::totalTargetTxns(0.0, 100, 30), 0.0));
  PL_CHECK(nearly(spenders::totalTargetTxns(60.0, 0, 30), 0.0));
  PL_CHECK(nearly(spenders::totalTargetTxns(60.0, 100, 0), 0.0));
  std::printf("  PASS: totalTargetTxns scaling\n");
}

} // namespace

int main() {
  std::printf("=== Spending Pipeline Tests ===\n");
  testChannelCdfShape();
  testChannelCdfWithUnknown();
  testChannelCdfFallsBackOnZeroCore();
  testChannelCdfClampsUnknown();
  testPickSlot();
  testCountFactorMonotonic();
  testCountFactorBoundaries();
  testMultiplierDisabled();
  testMultiplierStressRegion();
  testMultiplierBurdenPenalty();
  testTotalTargetTxns();
  std::printf("All spending pipeline tests passed.\n\n");
  return 0;
}
