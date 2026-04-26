#include "phantomledger/transfers/credit_cards/cycle.hpp"

#include <algorithm>
#include <cstdint>

namespace PhantomLedger::transfers::credit_cards {
namespace {

inline constexpr time::TimeOfDay kStatementCloseTime{23, 30, 0};

[[nodiscard]] unsigned clampDay(int year, unsigned month,
                                std::uint8_t requested) noexcept {
  const unsigned last = time::daysInMonth(year, month);
  return std::min<unsigned>(requested, last);
}

} // namespace

std::vector<time::TimePoint> statementCloseDates(time::TimePoint windowStart,
                                                 time::TimePoint windowEndExcl,
                                                 std::uint8_t cycleDay) {
  std::vector<time::TimePoint> closes;
  if (windowEndExcl <= windowStart || cycleDay == 0) {
    return closes;
  }

  // Twelve cycles covers a one-year window; almost no use case
  // exceeds this so it's a good default reservation.
  closes.reserve(13);

  const auto startDate = time::toCalendarDate(windowStart);
  int year = startDate.year;
  unsigned month = startDate.month;

  while (true) {
    const unsigned day = clampDay(year, month, cycleDay);
    const time::TimePoint close = time::makeTime(
        time::CalendarDate{year, month, day}, kStatementCloseTime);

    if (close >= windowEndExcl) {
      break;
    }
    if (close >= windowStart) {
      closes.push_back(close);
    }

    if (month == 12) {
      ++year;
      month = 1;
    } else {
      ++month;
    }
  }

  return closes;
}

time::TimePoint resolveDueDate(time::TimePoint due) noexcept {
  const int wd = time::weekday(due);
  time::TimePoint adjusted = due;
  if (wd == 5) {
    adjusted = time::addDays(due, 2); // Saturday -> Monday
  } else if (wd == 6) {
    adjusted = time::addDays(due, 1); // Sunday -> Monday
  }

  // Truncate sub-minute precision; due-date comparisons are minute-level.
  const auto date = time::toCalendarDate(adjusted);
  const auto tod = time::toTimeOfDay(adjusted);
  return time::makeTime(date, time::TimeOfDay{tod.hour, tod.minute, 0});
}

bool isOnTime(time::TimePoint paymentTimestamp, time::TimePoint due) noexcept {
  return paymentTimestamp <= resolveDueDate(due);
}

} // namespace PhantomLedger::transfers::credit_cards
