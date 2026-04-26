#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"

#include "test_support.hpp"

#include <cstdio>
#include <string>

using namespace PhantomLedger;

namespace {

[[nodiscard]] inline time::CalendarDate date(int year, unsigned month,
                                             unsigned day) {
  return time::CalendarDate{
      .year = year,
      .month = month,
      .day = day,
  };
}

[[nodiscard]] inline time::TimePoint at(int year, unsigned month, unsigned day,
                                        int hour = 0, int minute = 0,
                                        int second = 0) {
  return time::makeTime(date(year, month, day), time::TimeOfDay{
                                                    .hour = hour,
                                                    .minute = minute,
                                                    .second = second,
                                                });
}

void testMakeTime() {
  auto tp = at(2025, 1, 15, 10, 30, 45);
  auto cal = time::toCalendarDate(tp);
  auto tod = time::toTimeOfDay(tp);

  PL_CHECK_EQ(cal.year, 2025);
  PL_CHECK_EQ(cal.month, 1U);
  PL_CHECK_EQ(cal.day, 15U);
  PL_CHECK_EQ(tod.hour, 10);
  PL_CHECK_EQ(tod.minute, 30);
  PL_CHECK_EQ(tod.second, 45);
  std::printf("  PASS: makeTime / toCalendarDate / toTimeOfDay\n");
}

void testMakeTimeMidnight() {
  auto tp = at(2025, 6, 1);
  auto cal = time::toCalendarDate(tp);
  auto tod = time::toTimeOfDay(tp);

  PL_CHECK_EQ(cal.year, 2025);
  PL_CHECK_EQ(cal.month, 6U);
  PL_CHECK_EQ(cal.day, 1U);
  PL_CHECK_EQ(tod.hour, 0);
  PL_CHECK_EQ(tod.minute, 0);
  PL_CHECK_EQ(tod.second, 0);
  std::printf("  PASS: makeTime defaults to midnight\n");
}

void testMakeTimeInvalidDate() {
  PL_CHECK_THROWS(time::makeTime(date(2025, 2, 29))); // 2025 not leap year
  PL_CHECK_THROWS(time::makeTime(date(2025, 13, 1))); // invalid month
  PL_CHECK_THROWS(time::makeTime(date(2025, 0, 1)));  // month 0
  std::printf("  PASS: makeTime rejects invalid dates\n");
}

void testMakeTimeLeapYear() {
  auto tp = at(2024, 2, 29);
  auto cal = time::toCalendarDate(tp);
  PL_CHECK_EQ(cal.day, 29U);
  std::printf("  PASS: makeTime accepts Feb 29 on leap year\n");
}

void testParseYmd() {
  auto tp = time::parseYmd("2025-03-15");
  auto cal = time::toCalendarDate(tp);
  PL_CHECK_EQ(cal.year, 2025);
  PL_CHECK_EQ(cal.month, 3U);
  PL_CHECK_EQ(cal.day, 15U);

  PL_CHECK_THROWS(time::parseYmd("2025/03/15"));
  PL_CHECK_THROWS(time::parseYmd("25-03-15"));
  PL_CHECK_THROWS(time::parseYmd("not-a-date"));
  std::printf("  PASS: parseYmd\n");
}

void testWeekday() {
  // 2025-01-01 is a Wednesday; weekday is Mon=0..Sun=6.
  PL_CHECK_EQ(time::weekday(at(2025, 1, 1)), 2);
  PL_CHECK_EQ(time::weekday(at(2025, 1, 4)), 5); // Sat
  PL_CHECK_EQ(time::weekday(at(2025, 1, 5)), 6); // Sun
  PL_CHECK_EQ(time::weekday(at(2025, 1, 6)), 0); // Mon
  std::printf("  PASS: weekday (Mon=0..Sun=6)\n");
}

void testIsWeekend() {
  PL_CHECK(!time::isWeekend(at(2025, 1, 1))); // Wed
  PL_CHECK(!time::isWeekend(at(2025, 1, 3))); // Fri
  PL_CHECK(time::isWeekend(at(2025, 1, 4)));  // Sat
  PL_CHECK(time::isWeekend(at(2025, 1, 5)));  // Sun
  PL_CHECK(!time::isWeekend(at(2025, 1, 6))); // Mon
  std::printf("  PASS: isWeekend\n");
}

void testDaysInMonth() {
  PL_CHECK_EQ(time::daysInMonth(2025, 1), 31U);
  PL_CHECK_EQ(time::daysInMonth(2025, 2), 28U);
  PL_CHECK_EQ(time::daysInMonth(2024, 2), 29U); // leap
  PL_CHECK_EQ(time::daysInMonth(2025, 4), 30U);
  PL_CHECK_EQ(time::daysInMonth(2025, 12), 31U);
  std::printf("  PASS: daysInMonth\n");
}

void testMonthStart() {
  auto tp = at(2025, 3, 15, 14, 30, 0);
  auto ms = time::monthStart(tp);
  auto cal = time::toCalendarDate(ms);
  PL_CHECK_EQ(cal.year, 2025);
  PL_CHECK_EQ(cal.month, 3U);
  PL_CHECK_EQ(cal.day, 1U);

  auto tod = time::toTimeOfDay(ms);
  PL_CHECK_EQ(tod.hour, 0);
  PL_CHECK_EQ(tod.minute, 0);
  PL_CHECK_EQ(tod.second, 0);
  std::printf("  PASS: monthStart\n");
}

void testAddMonths() {
  auto base = at(2025, 1, 31, 10, 0, 0);

  // Jan 31 + 1 month -> Feb 28 (clamped, non-leap).
  auto feb = time::addMonths(base, 1);
  auto cal = time::toCalendarDate(feb);
  PL_CHECK_EQ(cal.month, 2U);
  PL_CHECK_EQ(cal.day, 28U);
  PL_CHECK_EQ(time::toTimeOfDay(feb).hour, 10); // time-of-day preserved

  // Jan 31 + 12 months -> Jan 31 next year.
  auto next = time::addMonths(base, 12);
  cal = time::toCalendarDate(next);
  PL_CHECK_EQ(cal.year, 2026);
  PL_CHECK_EQ(cal.month, 1U);
  PL_CHECK_EQ(cal.day, 31U);

  // Jan 31 + 0 months -> unchanged.
  PL_CHECK(time::addMonths(base, 0) == base);

  // Negative months step backward (the current implementation uses
  // chrono::months arithmetic, which accepts signed values; it does
  // not throw on a valid prior month).
  auto prev = time::addMonths(base, -1);
  cal = time::toCalendarDate(prev);
  PL_CHECK_EQ(cal.year, 2024);
  PL_CHECK_EQ(cal.month, 12U);
  PL_CHECK_EQ(cal.day, 31U);

  std::printf("  PASS: addMonths with day clamping (and signed delta)\n");
}

void testAddDays() {
  auto base = at(2025, 1, 1);
  auto result = time::addDays(base, 31);
  auto cal = time::toCalendarDate(result);
  PL_CHECK_EQ(cal.month, 2U);
  PL_CHECK_EQ(cal.day, 1U);
  std::printf("  PASS: addDays\n");
}

void testMonthStarts() {
  auto start = at(2025, 1, 15);
  auto end = at(2025, 4, 10);
  auto anchors = time::monthStarts(start, end);

  PL_CHECK_EQ(anchors.size(), 4U); // Jan, Feb, Mar, Apr
  PL_CHECK_EQ(time::toCalendarDate(anchors[0]).month, 1U);
  PL_CHECK_EQ(time::toCalendarDate(anchors[1]).month, 2U);
  PL_CHECK_EQ(time::toCalendarDate(anchors[2]).month, 3U);
  PL_CHECK_EQ(time::toCalendarDate(anchors[3]).month, 4U);

  for (const auto &a : anchors) {
    PL_CHECK_EQ(time::toCalendarDate(a).day, 1U);
  }

  auto empty = time::monthStarts(end, start);
  PL_CHECK(empty.empty());

  std::printf("  PASS: monthStarts\n");
}

void testClipHalfOpen() {
  auto w0 = at(2025, 1, 1);
  auto w1 = at(2025, 12, 31);
  auto a0 = at(2025, 3, 1);
  auto a1 = at(2025, 6, 1);

  auto result = time::clipHalfOpen(w0, w1, a0, a1);
  PL_CHECK(result.has_value());
  PL_CHECK(result->start == a0);
  PL_CHECK(result->endExcl == a1);

  auto early = time::clipHalfOpen(a0, a1, w0, w1);
  PL_CHECK(early.has_value());
  PL_CHECK(early->start == a0);
  PL_CHECK(early->endExcl == a1);

  auto before = at(2024, 1, 1);
  auto beforeEnd = at(2024, 6, 1);
  auto none = time::clipHalfOpen(w0, w1, before, beforeEnd);
  PL_CHECK(!none.has_value());

  auto open = time::clipHalfOpen(w0, w1, a0);
  PL_CHECK(open.has_value());
  PL_CHECK(open->start == a0);
  PL_CHECK(open->endExcl == w1);

  std::printf("  PASS: clipHalfOpen\n");
}

void testEpochConversion() {
  auto tp = at(2025, 1, 1, 0, 0, 0);
  auto epoch = time::toEpochSeconds(tp);
  auto roundTrip = time::fromEpochSeconds(epoch);
  PL_CHECK(roundTrip == tp);

  PL_CHECK_EQ(epoch, 1735689600LL); // 2025-01-01 00:00:00 UTC
  std::printf("  PASS: toEpochSeconds / fromEpochSeconds\n");
}

void testFormatTimestamp() {
  auto tp = at(2025, 3, 15, 9, 5, 7);
  auto s = time::formatTimestamp(tp);
  PL_CHECK_EQ(s, std::string("2025-03-15 09:05:07"));

  auto midnight = at(2025, 1, 1);
  PL_CHECK_EQ(time::formatTimestamp(midnight),
              std::string("2025-01-01 00:00:00"));

  std::printf("  PASS: formatTimestamp\n");
}

} // namespace

int main() {
  std::printf("=== Calendar Tests ===\n");
  testMakeTime();
  testMakeTimeMidnight();
  testMakeTimeInvalidDate();
  testMakeTimeLeapYear();
  testParseYmd();
  testWeekday();
  testIsWeekend();
  testDaysInMonth();
  testMonthStart();
  testAddMonths();
  testAddDays();
  testMonthStarts();
  testClipHalfOpen();
  testEpochConversion();
  testFormatTimestamp();
  std::printf("All calendar tests passed.\n\n");
  return 0;
}
