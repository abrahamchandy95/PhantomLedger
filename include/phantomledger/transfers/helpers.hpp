#pragma once
/*
 * Shared helpers used across the transfer modules.
 *
 * These exist in one place because every generator (government,
 * insurance, obligations, family, fraud, day-to-day) needs to:
 *
 *   - Convert a time::TimePoint into the int64 seconds that
 *     transactions::Draft::timestamp expects.
 *
 *   - Round money to two decimal places and clamp to non-negative.
 *
 * Keeping these inline in a single header avoids the alternative of
 * each generator re-implementing them and drifting in behavior.
 */

#include "phantomledger/primitives/time/calendar.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>

namespace PhantomLedger::transfers::helpers {

/// Seconds-since-epoch as an int64, matching the Draft timestamp
/// contract. Uses a seconds-resolution TimePoint so the duration cast
/// is a no-op.
[[nodiscard]] constexpr std::int64_t
draftTimestamp(time::TimePoint tp) noexcept {
  return tp.time_since_epoch().count();
}

/// Round an amount to two decimal places, clamped to non-negative.
/// Matches the `round(max(0.0, float(amount)), 2)` idiom used
/// throughout the Python transfer layer.
[[nodiscard]] inline double roundMoney(double amount) noexcept {
  const double clipped = std::max(0.0, amount);
  return std::round(clipped * 100.0) / 100.0;
}

} // namespace PhantomLedger::transfers::helpers
