//
// tests/test_window_invariance.cpp
//
// Five-leg windowed-driver acceptance matrix (migration step 6).
//
// The experimental two-phase WindowedTransferDriver must produce the
// byte-identical corpus for any generation-window size:
//
//   windowed full range == 12-month == 6-month == 3-month == 1-month
//
// Every leg rebuilds a fresh, identically seeded world and runs Phase A
// -> exact realized candidate count L -> fraud -> Phase B through the same
// monthly settlement schedule. Legs differ ONLY in the driver's
// generation-window strategy. Comparison uses the full RunFingerprint with
// first-difference diagnostics. Thread count is pinned to 1; thread
// invariance is a separate gate (test_thread_invariance).
//
// When this matrix fails, run test_window_bisect first: its staged gates
// localize the divergence to the driver fold, the enriched world
// configuration, or the fraud boundary.
//

#include "window_leg_support.hpp"

#include <cstdint>
#include <cstdio>

int main() {
  std::printf("=== Windowed Two-Phase Driver Invariance ===\n");

  constexpr std::uint64_t seed = 20260717;

  pltest::pl::time::Window window;
  window.start = pltest::pl::time::makeTime({2015, 1, 1});
  window.days = 365 * 2;

  const auto poolSet = pltest::buildPoolSet(seed);

  pltest::LegOptions options;
  options.seed = seed;
  options.window = window;
  options.generationMonths = 0;

  pltest::announceLeg("full-range");
  const auto reference = pltest::runLeg(poolSet, options);
  pltest::printLeg("full-range", reference);

  // The gate is vacuous unless the phases it pins actually engaged.
  PL_CHECK(reference.fingerprint.rows > 0);
  PL_CHECK(reference.fingerprint.candidateRows > 0);
  PL_CHECK(reference.fingerprint.fraudRows > 0);
  PL_CHECK(reference.fingerprint.cardEvents > 0);
  PL_CHECK(reference.fingerprint.digest.size() == 64);

  const struct {
    int months;
    const char *label;
  } legs[] = {
      {12, "12-month generation windows"},
      {6, "6-month generation windows"},
      {3, "3-month generation windows"},
      {1, "1-month generation windows"},
  };

  for (const auto &spec : legs) {
    options.generationMonths = spec.months;
    pltest::announceLeg(spec.label);
    const auto leg = pltest::runLeg(poolSet, options);
    pltest::printLeg(spec.label, leg);
    pltest::checkLegMatches(spec.label, reference, leg);
  }

  std::printf("windowed driver invariance holds across full/12/6/3/1-month "
              "generation windows; digest %s\n",
              reference.fingerprint.digest.c_str());
  return 0;
}
