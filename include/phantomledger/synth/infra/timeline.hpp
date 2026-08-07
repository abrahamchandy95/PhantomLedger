#pragma once

#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace PhantomLedger::synth::infra::timeline {

[[nodiscard]] inline std::pair<time::TimePoint, time::TimePoint>
sampleSpan(random::Rng &rng, time::TimePoint start, std::int32_t days) {
  if (days <= 1) {
    return {start, start};
  }

  const auto span = static_cast<std::int32_t>(
      rng.uniformInt(1, static_cast<std::int64_t>(days) + 1));

  const auto maxStartOffset = days - span;
  const auto offset =
      maxStartOffset <= 0
          ? 0
          : static_cast<std::int32_t>(rng.uniformInt(
                0, static_cast<std::int64_t>(maxStartOffset) + 1));

  const auto firstSeen = start + time::Days{offset};
  const auto lastSeen = firstSeen + time::Days{span - 1};
  return {firstSeen, lastSeen};
}

/* `sampleShortSpan` — an interval bounded at min(days, 7) DAYS — used to live
 * here and was the tenure every shared device and shared address got. Against
 * a four-year window that is 0.48% of the run, so the one cross-person sharing
 * mechanism in the model carried 0.0065% of card-view rows: correctly wired,
 * correctly routed, and statistically invisible. Shared endpoints are now
 * ordinary chains, so nothing needs a seven-day episode and the function is
 * gone rather than left standing for the next reader to reach for. */

// ------------------------------------------------- REPLACEMENT CHAINS
//
// `sampleSpan` above places ONE independent interval inside the window,
// which is why it cannot be used for routing: independent intervals do
// not tile, so at a random instant a person can have ZERO live
// endpoints, and a router that filtered on them would have nothing to
// return ~27% of the time (measured, device-ip-lifecycle).
//
// `sampleChain` instead partitions [start, start+days) into CONSECUTIVE
// tenures, so exactly one endpoint on the line is live at every instant
// in the window. Half-open and gapless by construction: each tenure's
// end IS the next tenure's start, so no timestamp can fall between two
// links.
//
// The first tenure is deliberately TRUNCATED by a uniform draw. Without
// it every person on the planet would replace their first device on the
// same day, and "days since device first seen" would be a global clock
// rather than a per-person feature. The truncation makes the phase
// independent across people, which is what a real replacement cohort
// looks like.
//
// `meanDaysAt` is called with the tenure's OWN start, so a chain
// crossing eras picks up each era's replacement rate as it goes — the
// 1991 links are long and the 2015 links are short within one person's
// history.

struct Link {
  time::TimePoint firstSeen;    // inclusive
  time::TimePoint lastSeenExcl; // exclusive
};

template <typename MeanDaysAt>
[[nodiscard]] inline std::vector<Link>
sampleChain(random::Rng &rng, time::TimePoint start, std::int32_t days,
            MeanDaysAt meanDaysAt) {
  std::vector<Link> out;
  if (days <= 0) {
    return out;
  }

  const auto endExcl = start + time::Days{days};

  // Dispersion around the era mean. Lognormal-shaped via a ratio draw
  // in [0.55, 1.45]: heavy enough that tenure is not a lattice, tight
  // enough that no single link swallows a multi-decade window.
  const auto drawTenure = [&](time::TimePoint at) {
    const auto mean = meanDaysAt(at);
    const auto lo = static_cast<std::int64_t>(mean * 0.55);
    const auto hi = static_cast<std::int64_t>(mean * 1.45);
    const auto span = hi > lo ? hi - lo : 1;
    return static_cast<std::int32_t>(
        std::max<std::int64_t>(1, lo + rng.uniformInt(0, span + 1)));
  };

  auto cursor = start;

  // Phase the cohort: the first link is a uniform fraction of a full
  // tenure, so replacement dates are independent across people.
  {
    const auto full = drawTenure(cursor);
    const auto phase = static_cast<std::int32_t>(
        rng.uniformInt(1, static_cast<std::int64_t>(full) + 1));
    auto next = cursor + time::Days{phase};
    if (next > endExcl) {
      next = endExcl;
    }
    out.push_back(Link{cursor, next});
    cursor = next;
  }

  while (cursor < endExcl) {
    const auto tenure = drawTenure(cursor);
    auto next = cursor + time::Days{tenure};
    if (next > endExcl) {
      next = endExcl;
    }
    out.push_back(Link{cursor, next});
    cursor = next;
  }

  return out;
}

} // namespace PhantomLedger::synth::infra::timeline
