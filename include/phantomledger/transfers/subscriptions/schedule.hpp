#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

#include <algorithm>
#include <cstdint>

namespace PhantomLedger::transfers::subscriptions {

inline constexpr int kDayMin = 1;
inline constexpr int kDayMax = 28;

inline constexpr int kHourLow = 0;
inline constexpr int kHourHighExcl = 7;
inline constexpr int kMinuteLow = 0;
inline constexpr int kMinuteHighExcl = 61;

/// Scheduling knobs for posting a materialized subscription in each month.
struct ScheduleRules {
  std::int32_t dayJitter = 1;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::ge("subscriptions.dayJitter", dayJitter, 0); });
  }

  void validate() const {
    primitives::validate::Report r;
    validate(r);
    r.throwIfFailed();
  }
};

[[nodiscard]] inline std::int64_t
cycleTimestamp(random::Rng &rng, time::TimePoint monthStart,
               std::uint8_t baseDay, const ScheduleRules &rules) noexcept {
  const auto dayJitter = static_cast<std::uint8_t>(rules.dayJitter);
  const auto jitter =
      (dayJitter == 0U)
          ? std::int64_t{0}
          : rng.uniformInt(-static_cast<std::int64_t>(dayJitter),
                           static_cast<std::int64_t>(dayJitter) + 1);
  const int rawDay = static_cast<int>(baseDay) + static_cast<int>(jitter);
  const int day = std::clamp(rawDay, kDayMin, kDayMax);

  const auto hour = rng.uniformInt(kHourLow, kHourHighExcl);
  const auto minute = rng.uniformInt(kMinuteLow, kMinuteHighExcl);

  const auto tp = monthStart + time::Days{day - 1} + time::Hours{hour} +
                  time::Minutes{minute};
  return time::toEpochSeconds(tp);
}

} // namespace PhantomLedger::transfers::subscriptions
