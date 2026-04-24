#pragma once
/*
 * inflows/revenue/clock.hpp — business-day timestamp sampling.
 *
 * Given a month anchor, samples a timestamp on a weekday within
 * the month, retrying up to 16 times to avoid weekends. If all
 * retries land on weekends, rolls forward to the next Monday.
 */

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/inflows/revenue/draw.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

#include <algorithm>

namespace PhantomLedger::inflows::revenue {

struct BusinessDayWindow {
  int earliestHour = 9;
  int latestHour = 17;
  int startDay = 0;
  int endDayExclusive = 28;

  [[nodiscard]] constexpr bool valid() const noexcept {
    return earliestHour >= 0 && earliestHour <= latestHour &&
           latestHour <= 23 && startDay >= 0 && startDay <= 27 &&
           endDayExclusive >= startDay + 1 && endDayExclusive <= 28;
  }
};

namespace detail {

[[nodiscard]] constexpr BusinessDayWindow
clampWindow(BusinessDayWindow window) noexcept {
  window.earliestHour = std::clamp(window.earliestHour, 0, 23);
  window.latestHour = std::clamp(window.latestHour, window.earliestHour, 23);
  window.startDay = std::clamp(window.startDay, 0, 27);
  window.endDayExclusive =
      std::max(window.startDay + 1, std::min(28, window.endDayExclusive));
  return window;
}

[[nodiscard]] inline time::TimePoint
sampledTimestamp(time::TimePoint monthStart, int day, int hour, int minute) {
  return time::addDays(monthStart, day) + time::Hours{hour} +
         time::Minutes{minute};
}

[[nodiscard]] inline time::TimePoint
rolledBusinessDay(time::TimePoint monthStart, random::Rng &rng,
                  const BusinessDayWindow &window) {
  auto ts = time::addDays(
      monthStart, randInt(rng, window.startDay, window.endDayExclusive));

  while (time::isWeekend(ts)) {
    ts = time::addDays(ts, 1);
  }

  const int minute = randInt(rng, 0, 60);
  const auto date = time::toCalendarDate(ts);

  return time::makeTime(
      time::CalendarDate{
          .year = date.year,
          .month = date.month,
          .day = date.day,
      },
      time::TimeOfDay{
          .hour = window.earliestHour,
          .minute = minute,
          .second = 0,
      });
}

} // namespace detail

/// Sample a business-day timestamp within
/// [monthStart + startDay, monthStart + endDayExclusive),
/// at an hour within [earliestHour, latestHour].
[[nodiscard]] inline time::TimePoint businessDayTs(time::TimePoint monthStart,
                                                   random::Rng &rng,
                                                   BusinessDayWindow window) {
  window = detail::clampWindow(window);

  for (int attempt = 0; attempt < 16; ++attempt) {
    const int day = randInt(rng, window.startDay, window.endDayExclusive);
    const int hour = randInt(rng, window.earliestHour, window.latestHour + 1);
    const int minute = randInt(rng, 0, 60);

    const auto ts = detail::sampledTimestamp(monthStart, day, hour, minute);
    if (!time::isWeekend(ts)) {
      return ts;
    }
  }

  return detail::rolledBusinessDay(monthStart, rng, window);
}

} // namespace PhantomLedger::inflows::revenue
