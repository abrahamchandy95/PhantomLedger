#pragma once
/*
 * calendar.hpp — date arithmetic and calendar iteration.
 */

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <stdexcept>
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

// -------------------------------------------------------------------
// Calendar components
// -------------------------------------------------------------------

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

// -------------------------------------------------------------------
// Construction
// -------------------------------------------------------------------

[[nodiscard]] inline TimePoint makeTime(int year, unsigned month, unsigned day,
                                        int hour = 0, int minute = 0,
                                        int second = 0) {
  const std::chrono::year_month_day ymd{
      std::chrono::year{year},
      std::chrono::month{month},
      std::chrono::day{day},
  };
  if (!ymd.ok()) {
    throw std::invalid_argument("invalid calendar date");
  }
  const auto sysDay = std::chrono::sys_days{ymd};
  return std::chrono::time_point_cast<Seconds>(sysDay) + Hours{hour} +
         Minutes{minute} + Seconds{second};
}

[[nodiscard]] inline TimePoint parseYmd(std::string_view s) {
  if (s.size() != 10 || s[4] != '-' || s[7] != '-') {
    throw std::invalid_argument("date must be YYYY-MM-DD");
  }
  const int y = std::stoi(std::string(s.substr(0, 4)));
  const auto m = static_cast<unsigned>(std::stoi(std::string(s.substr(5, 2))));
  const auto d = static_cast<unsigned>(std::stoi(std::string(s.substr(8, 2))));
  return makeTime(y, m, d);
}

// -------------------------------------------------------------------
// Component extraction
// -------------------------------------------------------------------

[[nodiscard]] inline CalendarDate toCalendarDate(TimePoint tp) {
  const auto dp = std::chrono::floor<Days>(tp);
  const std::chrono::year_month_day ymd{dp};
  return {
      static_cast<int>(ymd.year()),
      static_cast<unsigned>(ymd.month()),
      static_cast<unsigned>(ymd.day()),
  };
}

[[nodiscard]] inline TimeOfDay toTimeOfDay(TimePoint tp) {
  const auto dp = std::chrono::floor<Days>(tp);
  const auto tod = tp - dp;
  const auto h = std::chrono::duration_cast<Hours>(tod);
  const auto m = std::chrono::duration_cast<Minutes>(tod - h);
  const auto s = std::chrono::duration_cast<Seconds>(tod - h - m);
  return {
      static_cast<int>(h.count()),
      static_cast<int>(m.count()),
      static_cast<int>(s.count()),
  };
}

/// Monday=0 .. Sunday=6 (matches Python weekday()).
[[nodiscard]] inline int weekday(TimePoint tp) {
  const auto dp = std::chrono::floor<Days>(tp);
  const std::chrono::weekday wd{dp};
  // c_encoding: Sunday=0 Mon=1 .. Sat=6
  // iso_encoding: Mon=1 .. Sun=7
  // We want Mon=0 .. Sun=6
  unsigned iso = wd.iso_encoding(); // 1..7
  return static_cast<int>(iso) - 1; // 0..6
}

[[nodiscard]] inline bool isWeekend(TimePoint tp) { return weekday(tp) >= 5; }

// -------------------------------------------------------------------
// Date arithmetic
// -------------------------------------------------------------------

[[nodiscard]] inline unsigned daysInMonth(int year, unsigned month) {
  const std::chrono::year_month_day_last ymdl{
      std::chrono::year{year} / std::chrono::month{month} / std::chrono::last};
  return static_cast<unsigned>(ymdl.day());
}

/// Return midnight of the first day of tp's month.
[[nodiscard]] inline TimePoint monthStart(TimePoint tp) {
  const auto cal = toCalendarDate(tp);
  return makeTime(cal.year, cal.month, 1);
}

/// Add calendar months, clamping the day to the target month's range.
/// Preserves time-of-day.
[[nodiscard]] inline TimePoint addMonths(TimePoint tp, int months) {
  if (months < 0) {
    throw std::invalid_argument("addMonths: months must be >= 0");
  }
  const auto cal = toCalendarDate(tp);
  const int rawMonth = (static_cast<int>(cal.month) - 1) + months;
  const int newYear = cal.year + rawMonth / 12;
  const auto newMonth = static_cast<unsigned>((rawMonth % 12) + 1);
  const unsigned maxDay = daysInMonth(newYear, newMonth);
  const unsigned newDay = std::min(cal.day, maxDay);

  // Preserve time-of-day.
  const auto dp = std::chrono::floor<Days>(tp);
  const auto dayPart = tp - dp;
  return makeTime(newYear, newMonth, newDay) +
         std::chrono::duration_cast<Seconds>(dayPart);
}

/// Add days.
[[nodiscard]] inline TimePoint addDays(TimePoint tp, int days) {
  return tp + Days{days};
}

// -------------------------------------------------------------------
// Iteration
// -------------------------------------------------------------------

/// Yield first-of-month anchors covering [start, endExcl).
[[nodiscard]] inline std::vector<TimePoint> monthStarts(TimePoint start,
                                                        TimePoint endExcl) {
  std::vector<TimePoint> anchors;
  auto current = monthStart(start);
  while (current < endExcl) {
    anchors.push_back(current);
    current = addMonths(current, 1);
  }
  return anchors;
}

// -------------------------------------------------------------------
// Half-open interval intersection
// -------------------------------------------------------------------

struct HalfOpenInterval {
  TimePoint start;
  TimePoint endExcl;
};

[[nodiscard]] inline std::optional<HalfOpenInterval>
clipHalfOpen(TimePoint windowStart, TimePoint windowEndExcl,
             TimePoint activeStart,
             std::optional<TimePoint> activeEndExcl = std::nullopt) {
  if (windowEndExcl <= windowStart)
    return std::nullopt;
  if (activeEndExcl.has_value() && *activeEndExcl <= activeStart)
    return std::nullopt;

  const auto s = std::max(windowStart, activeStart);
  const auto e = activeEndExcl.has_value()
                     ? std::min(windowEndExcl, *activeEndExcl)
                     : windowEndExcl;
  if (s >= e)
    return std::nullopt;

  return HalfOpenInterval{s, e};
}

// -------------------------------------------------------------------
// Epoch conversion (Transaction.timestamp interop)
// -------------------------------------------------------------------

[[nodiscard]] inline std::int64_t toEpochSeconds(TimePoint tp) {
  return std::chrono::duration_cast<Seconds>(tp.time_since_epoch()).count();
}

[[nodiscard]] inline TimePoint fromEpochSeconds(std::int64_t epoch) {
  return TimePoint{Seconds{epoch}};
}

// -------------------------------------------------------------------
// Formatting (ISO 8601 for CSV)
// -------------------------------------------------------------------

[[nodiscard]] inline std::string formatTimestamp(TimePoint tp) {
  const auto cal = toCalendarDate(tp);
  const auto tod = toTimeOfDay(tp);
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%04d-%02u-%02u %02d:%02d:%02d", cal.year,
                cal.month, cal.day, tod.hour, tod.minute, tod.second);
  return std::string(buf);
}

} // namespace PhantomLedger::time
