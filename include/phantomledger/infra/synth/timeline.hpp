#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace PhantomLedger::infra::synth::timeline {

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

[[nodiscard]] inline std::pair<time::TimePoint, time::TimePoint>
sampleShortSpan(random::Rng &rng, time::TimePoint start, std::int32_t days) {
  const auto bound = std::min<std::int32_t>(days, 7);
  if (bound <= 1) {
    return {start, start};
  }

  const auto span = static_cast<std::int32_t>(
      rng.uniformInt(1, static_cast<std::int64_t>(bound) + 1));

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

} // namespace PhantomLedger::infra::synth::timeline
