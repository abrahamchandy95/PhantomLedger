//
// tests/test_window_bisect.cpp
//
// Staged bisection for windowed-driver divergences.
//
// Each gate compares a full-range leg against a 1-month-generation leg —
// the harshest partition — with one layer of the system enabled at a time.
// A failure therefore names the layer that introduced the window-size
// dependence:
//
//   gate A  minimal: session + card lifecycle through the pre-fraud fold.
//           No device/IP routing, no income, no products, no fraud.
//           Fails -> the driver fold or the session's cross-window state.
//
//   gate B  enriched Phase A: adds device/IP routing draws on the shared
//           RNG, the income cursor source, and the product schedule.
//           A passes, B fails -> a source or the routing interleaving.
//
//   gate C  full two-phase: adds the fraud boundary and Phase B.
//           A and B pass, C fails -> fraud generation or the post-fraud
//           fold.
//
// Gates run in order and abort at the first failure, so the last printed
// gate is the failing layer.
//

#include "window_leg_support.hpp"

#include <cstdint>
#include <cstdio>

namespace {

void runGate(const char *name, const pltest::pl::synth::pii::PoolSet &poolSet,
             pltest::LegOptions options, bool requireFraud) {
  std::printf("--- %s ---\n", name);
  std::fflush(stdout);

  options.generationMonths = 0;
  pltest::announceLeg("full-range");
  const auto reference = pltest::runLeg(poolSet, options);
  pltest::printLeg("full-range", reference);

  PL_CHECK(reference.fingerprint.rows > 0);
  PL_CHECK(reference.fingerprint.candidateRows > 0);
  PL_CHECK(reference.fingerprint.cardEvents > 0);
  if (requireFraud) {
    PL_CHECK(reference.fingerprint.fraudRows > 0);
  }

  options.generationMonths = 1;
  pltest::announceLeg("1-month windows");
  const auto monthly = pltest::runLeg(poolSet, options);
  pltest::printLeg("1-month windows", monthly);

  pltest::checkLegMatches(name, reference, monthly);
}

} // namespace

int main() {
  std::printf("=== Windowed Driver Bisection ===\n");

  constexpr std::uint64_t seed = 20260717;

  pltest::pl::time::Window window;
  window.start = pltest::pl::time::makeTime({2015, 1, 1});
  window.days = 365 * 2;

  const auto poolSet = pltest::buildPoolSet(seed);

  pltest::LegOptions base;
  base.seed = seed;
  base.window = window;

  {
    auto options = base;
    options.withInfraRouting = false;
    options.withIncome = false;
    options.withProducts = false;
    options.withFraud = false;
    runGate("gate A: minimal session fold", poolSet, options,
            /*requireFraud=*/false);
  }

  {
    auto options = base;
    options.withFraud = false;
    runGate("gate B: enriched Phase A (routing + income + products)", poolSet,
            options, /*requireFraud=*/false);
  }

  {
    auto options = base;
    runGate("gate C: full two-phase with fraud", poolSet, options,
            /*requireFraud=*/true);
  }

  std::printf("all bisection gates hold: driver fold, enriched Phase A and "
              "the fraud boundary are window-invariant\n");
  return 0;
}
