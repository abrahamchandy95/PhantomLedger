//
// tests/test_thread_invariance.cpp
//
// Thread-count invariance gate (migration step 7).
//
// Work shards may steer WHERE work happens, never WHAT bytes are generated
// (pipeline/batch/cogen.hpp, ROLE 1). This gate enforces that claim: the
// windowed two-phase driver must produce the byte-identical corpus with 1,
// 4 and 12 emission workers.
//
// Per the acceptance plan, thread-count variation is NOT combined with
// window-size variation: the generation window is pinned to 3 months for
// every leg, and each leg rebuilds a fresh, identically seeded world.
// Window-size invariance is test_window_invariance's job.
//

#include "window_leg_support.hpp"

#include <cstdint>
#include <cstdio>

int main() {
  std::printf("=== Thread-Count Invariance (3-month windows) ===\n");

  constexpr std::uint64_t seed = 20260718;

  pltest::pl::time::Window window;
  window.start = pltest::pl::time::makeTime({2015, 1, 1});
  window.days = 365 * 2;

  const auto poolSet = pltest::buildPoolSet(seed);

  pltest::LegOptions options;
  options.seed = seed;
  options.window = window;
  options.generationMonths = 3;
  options.threadCount = 1;

  pltest::announceLeg("1 worker");
  const auto reference = pltest::runLeg(poolSet, options);
  pltest::printLeg("1 worker", reference);

  PL_CHECK(reference.fingerprint.rows > 0);
  PL_CHECK(reference.fingerprint.candidateRows > 0);
  PL_CHECK(reference.fingerprint.fraudRows > 0);
  PL_CHECK(reference.fingerprint.cardEvents > 0);

  const struct {
    std::uint32_t workers;
    const char *label;
  } legs[] = {
      {4, "4 workers"},
      {12, "12 workers"},
  };

  for (const auto &spec : legs) {
    options.threadCount = spec.workers;
    pltest::announceLeg(spec.label);
    const auto leg = pltest::runLeg(poolSet, options);
    pltest::printLeg(spec.label, leg);
    pltest::checkLegMatches(spec.label, reference, leg);
  }

  std::printf("thread-count invariance holds for 1/4/12 workers; digest %s\n",
              reference.fingerprint.digest.c_str());
  return 0;
}
