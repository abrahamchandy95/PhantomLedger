#include "phantomledger/primitives/time/calendar.hpp"

#include <algorithm>
#include <cstdio>
#include <stdexcept>

namespace PhantomLedger::time {

TimePoint makeTime(CalendarDate date, TimeOfDay time) {
  const auto ymd = std::chrono::year_month_day{
      std::chrono::year{date.year},
      std::chrono::month{date.month},
      std::chrono::day{date.day},
  };

  if (!ymd.ok()) {
    throw std::invalid_argument("invalid calendar date");
  }

  return std::chrono::time_point_cast<Seconds>(std::chrono::sys_days{ymd}) +
         Hours{time.hour} + Minutes{time.minute} + Seconds{time.second};
}

TimePoint parseYmd(std::string_view s) {
  if (s.size() != 10 || s[4] != '-' || s[7] != '-') {
    throw std::invalid_argument("date must be YYYY-MM-DD");
  }

  const CalendarDate date{
      .year = std::stoi(std::string(s.substr(0, 4))),
      .month = static_cast<unsigned>(std::stoi(std::string(s.substr(5, 2)))),
      .day = static_cast<unsigned>(std::stoi(std::string(s.substr(8, 2)))),
  };

  return makeTime(date);
}

CalendarDate toCalendarDate(TimePoint tp) {
  const auto dp = std::chrono::floor<Days>(tp);
  const std::chrono::year_month_day ymd{dp};
  return {
      static_cast<int>(ymd.year()),
      static_cast<unsigned>(ymd.month()),
      static_cast<unsigned>(ymd.day()),
  };
}

TimeOfDay toTimeOfDay(TimePoint tp) {
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

int weekday(TimePoint tp) {
  const auto dp = std::chrono::floor<Days>(tp);
  const std::chrono::weekday wd{dp};
  // iso_encoding: Mon=1 .. Sun=7; we want Mon=0 .. Sun=6.
  const unsigned iso = wd.iso_encoding();
  return static_cast<int>(iso) - 1;
}

bool isWeekend(TimePoint tp) { return weekday(tp) >= 5; }

unsigned daysInMonth(int year, unsigned month) {
  const std::chrono::year_month_day_last ymdl{
      std::chrono::year{year} / std::chrono::month{month} / std::chrono::last};
  return static_cast<unsigned>(ymdl.day());
}

TimePoint monthStart(TimePoint tp) {
  const auto cal = toCalendarDate(tp);
  return makeTime(CalendarDate{
      .year = cal.year,
      .month = cal.month,
      .day = 1,
  });
}

TimePoint addMonths(TimePoint tp, int months) {
  const auto date = toCalendarDate(tp);
  const auto dayPoint = std::chrono::floor<Days>(tp);
  const auto timeOfDay = tp - dayPoint;

  const auto first = std::chrono::year{date.year} /
                     std::chrono::month{date.month} / std::chrono::day{1};

  const auto shifted =
      std::chrono::year_month_day{first + std::chrono::months{months}};
  if (!shifted.ok()) {
    throw std::invalid_argument("addMonths: invalid shifted month");
  }

  const auto year = static_cast<int>(shifted.year());
  const auto month = static_cast<unsigned>(shifted.month());
  const auto day = std::min(date.day, daysInMonth(year, month));

  return makeTime(CalendarDate{
             .year = year,
             .month = month,
             .day = day,
         }) +
         timeOfDay;
}

std::vector<TimePoint> monthStarts(TimePoint start, TimePoint endExcl) {
  std::vector<TimePoint> anchors;
  auto current = monthStart(start);
  while (current < endExcl) {
    anchors.push_back(current);
    current = addMonths(current, 1);
  }
  return anchors;
}

std::optional<HalfOpenInterval>
clipHalfOpen(TimePoint windowStart, TimePoint windowEndExcl,
             TimePoint activeStart, std::optional<TimePoint> activeEndExcl) {
  if (windowEndExcl <= windowStart) {
    return std::nullopt;
  }
  if (activeEndExcl.has_value() && *activeEndExcl <= activeStart) {
    return std::nullopt;
  }

  const auto s = std::max(windowStart, activeStart);
  const auto e = activeEndExcl.has_value()
                     ? std::min(windowEndExcl, *activeEndExcl)
                     : windowEndExcl;
  if (s >= e) {
    return std::nullopt;
  }

  return HalfOpenInterval{s, e};
}

std::string formatTimestamp(TimePoint tp) {
  const auto cal = toCalendarDate(tp);
  const auto tod = toTimeOfDay(tp);
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%04d-%02u-%02u %02d:%02d:%02d", cal.year,
                cal.month, cal.day, tod.hour, tod.minute, tod.second);
  return std::string(buf);
}

} // namespace PhantomLedger::time
