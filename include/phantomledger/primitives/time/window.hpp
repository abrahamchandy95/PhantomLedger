#pragma once
/*
 * Window — a half-open temporal range used as a parameter bundle
 * across the simulation. Plus the small set of operations that work
 * on intervals rather than single dates.
 */

#include "phantomledger/primitives/time/calendar.hpp"

#include <optional>
#include <vector>

namespace PhantomLedger::time {

struct Window {
  TimePoint start{};
  int days = 0;

  [[nodiscard]] TimePoint endExcl() const noexcept {
    return start + Days{days};
  }
};

/// Half-open interval expressed as two TimePoints, returned by
/// clipping operations.
struct HalfOpenInterval {
  TimePoint start;
  TimePoint endExcl;
};

/// Midnight on the first day of every month overlapping [start, endExcl).
[[nodiscard]] std::vector<TimePoint> monthStarts(TimePoint start,
                                                 TimePoint endExcl);

/// Intersection of [windowStart, windowEndExcl) with
/// [activeStart, activeEndExcl?). Returns nullopt for empty overlap.
[[nodiscard]] std::optional<HalfOpenInterval>
clipHalfOpen(TimePoint windowStart, TimePoint windowEndExcl,
             TimePoint activeStart,
             std::optional<TimePoint> activeEndExcl = std::nullopt);

} // namespace PhantomLedger::time
