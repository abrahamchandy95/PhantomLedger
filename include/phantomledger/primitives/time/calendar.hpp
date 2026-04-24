#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace PhantomLedger::time {

using Clock = std::chrono::system_clock;
using Days = std::chrono::days;
using Hours = std::chrono::hours;
using Minutes = std::chrono::minutes;
using Seconds = std::chrono::seconds;
using TimePoint = std::chrono::time_point<Clock, Seconds>;

struct CalendarDate {
  int year;
  unsigned month;
  unsigned day;
};

struct TimeOfDay {
  int hour;
  int minute;
  int second;
};

struct HalfOpenInterval {
  TimePoint start;
  TimePoint endExcl;
};

// Construction.
[[nodiscard]] TimePoint makeTime(CalendarDate date, TimeOfDay time = {0, 0, 0});
[[nodiscard]] TimePoint parseYmd(std::string_view s);

// Component extraction.
[[nodiscard]] CalendarDate toCalendarDate(TimePoint tp);
[[nodiscard]] TimeOfDay toTimeOfDay(TimePoint tp);

/// Monday=0 .. Sunday=6 (matches Python weekday()).
[[nodiscard]] int weekday(TimePoint tp);
[[nodiscard]] bool isWeekend(TimePoint tp);

// Date arithmetic.
[[nodiscard]] unsigned daysInMonth(int year, unsigned month);
[[nodiscard]] TimePoint monthStart(TimePoint tp);
[[nodiscard]] TimePoint addMonths(TimePoint tp, int months);

[[nodiscard]] inline TimePoint addDays(TimePoint tp, int days) {
  return tp + Days{days};
}

// Iteration.
[[nodiscard]] std::vector<TimePoint> monthStarts(TimePoint start,
                                                 TimePoint endExcl);

// Half-open interval intersection.
[[nodiscard]] std::optional<HalfOpenInterval>
clipHalfOpen(TimePoint windowStart, TimePoint windowEndExcl,
             TimePoint activeStart,
             std::optional<TimePoint> activeEndExcl = std::nullopt);

// Epoch conversion.
[[nodiscard]] inline std::int64_t toEpochSeconds(TimePoint tp) {
  return std::chrono::duration_cast<Seconds>(tp.time_since_epoch()).count();
}

[[nodiscard]] inline TimePoint fromEpochSeconds(std::int64_t epoch) {
  return TimePoint{Seconds{epoch}};
}

// Formatting.
[[nodiscard]] std::string formatTimestamp(TimePoint tp);

} // namespace PhantomLedger::time
