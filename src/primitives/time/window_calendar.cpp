#include "phantomledger/primitives/time/window_calendar.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace PhantomLedger::time {
namespace {

// Fixed IRS quarterly estimated-tax dates: prior-Q4, Q1, Q2, Q3.
// (The nominal Q4 due date lands in mid-January of the following year.)
constexpr std::array<std::pair<int, int>, 4> kQuarterlyTaxDates{{
    {1, 15},
    {4, 15},
    {6, 15},
    {9, 15},
}};

/// Days since epoch, signed. Monotone in time, suitable as a set key.
[[nodiscard]] std::int64_t dayKey(CalendarDate date) noexcept {
  const auto ymd = std::chrono::year_month_day{std::chrono::year{date.year},
                                               std::chrono::month{date.month},
                                               std::chrono::day{date.day}};
  const auto sd = std::chrono::sys_days{ymd};
  return static_cast<std::int64_t>(sd.time_since_epoch().count());
}

/// Weekday of a CalendarDate using the same convention as
/// time::weekday(TimePoint): Mon=0 .. Sun=6.
[[nodiscard]] int weekdayOf(CalendarDate date) noexcept {
  const auto ymd = std::chrono::year_month_day{std::chrono::year{date.year},
                                               std::chrono::month{date.month},
                                               std::chrono::day{date.day}};
  const std::chrono::weekday wd{std::chrono::sys_days{ymd}};
  return static_cast<int>(wd.iso_encoding()) - 1;
}

/// nth occurrence of a given weekday (Mon=0..Sun=6) in the given
/// year/month. `occurrence` is 1-based. Returns {0,0,0} and sets
/// `ok` to false if the occurrence lands past the month's end.
[[nodiscard]] CalendarDate nthWeekdayOfMonth(int year, unsigned month,
                                             int weekday, int occurrence,
                                             bool &ok) noexcept {
  const CalendarDate first{year, month, 1u};
  const int wd0 = weekdayOf(first);
  const int delta = ((weekday - wd0) % 7 + 7) % 7;
  const unsigned day = 1u + static_cast<unsigned>(delta) +
                       7u * static_cast<unsigned>(occurrence - 1);

  const auto lastDom = daysInMonth(year, month);
  if (day > lastDom) {
    ok = false;
    return {};
  }
  ok = true;
  return {year, month, day};
}

/// Last occurrence of a given weekday in the given year/month.
[[nodiscard]] CalendarDate lastWeekdayOfMonth(int year, unsigned month,
                                              int weekday) noexcept {
  const auto lastDom = daysInMonth(year, month);
  const CalendarDate last{year, month, lastDom};
  const int wdLast = weekdayOf(last);
  const int delta = ((wdLast - weekday) % 7 + 7) % 7;
  return {year, month, lastDom - static_cast<unsigned>(delta)};
}

/// Shifts Saturday/Sunday holidays to the nearest weekday using the
/// federal OPM observed-holiday rule: Sat -> prior Fri, Sun -> Mon.
[[nodiscard]] CalendarDate observedFixedHoliday(int year, unsigned month,
                                                unsigned day) noexcept {
  const CalendarDate actual{year, month, day};
  const int wd = weekdayOf(actual);
  if (wd == 5) { // Saturday
    // Previous day. Always in the same month unless the 1st of a
    // month happens to be Saturday, which is fine because then we
    // shift to the last day of the prior month (still a valid date).
    auto tp = makeTime(actual);
    tp -= Days{1};
    return toCalendarDate(tp);
  }
  if (wd == 6) { // Sunday
    auto tp = makeTime(actual);
    tp += Days{1};
    return toCalendarDate(tp);
  }
  return actual;
}

/// Build the set of US federal holiday dates for a single year as
/// day-count keys. Compressed into one function for locality.
[[nodiscard]] std::unordered_set<std::int64_t> buildFederalHolidays(int year) {
  std::unordered_set<std::int64_t> h;
  h.reserve(11);

  h.insert(dayKey(observedFixedHoliday(year, 1, 1)));

  bool ok = true;
  auto d = nthWeekdayOfMonth(year, 1, /*mon=*/0, 3, ok);
  if (ok) {
    h.insert(dayKey(d));
  }
  d = nthWeekdayOfMonth(year, 2, 0, 3, ok);
  if (ok) {
    h.insert(dayKey(d));
  }

  h.insert(dayKey(lastWeekdayOfMonth(year, 5, 0))); // Memorial Day
  h.insert(dayKey(observedFixedHoliday(year, 6, 19)));
  h.insert(dayKey(observedFixedHoliday(year, 7, 4)));

  d = nthWeekdayOfMonth(year, 9, 0, 1, ok);
  if (ok) {
    h.insert(dayKey(d));
  }
  d = nthWeekdayOfMonth(year, 10, 0, 2, ok);
  if (ok) {
    h.insert(dayKey(d));
  }

  h.insert(dayKey(observedFixedHoliday(year, 11, 11)));

  d = nthWeekdayOfMonth(year, 11, /*thu=*/3, 4, ok);
  if (ok) {
    h.insert(dayKey(d));
  }

  h.insert(dayKey(observedFixedHoliday(year, 12, 25)));

  return h;
}

/// SSA cycle payment date: nth Wednesday of month, stepped backward
/// one day at a time until landing on a non-holiday weekday.
[[nodiscard]] CalendarDate ssaCyclePaymentDate(
    int year, unsigned month, int birthDay,
    std::unordered_map<int, std::unordered_set<std::int64_t>> &holidayCache) {
  int occurrence = 4;
  if (birthDay <= 10) {
    occurrence = 2;
  } else if (birthDay <= 20) {
    occurrence = 3;
  }

  bool ok = true;
  auto date = nthWeekdayOfMonth(year, month, /*wed=*/2, occurrence, ok);
  if (!ok) {
    // Extremely rare: a given month doesn't have an Nth Wednesday
    // because of its layout. Fall back to the last Wednesday.
    date = lastWeekdayOfMonth(year, month, 2);
  }

  // Walk backward over holidays/weekends until a business day is found.
  auto tp = makeTime(date);
  while (true) {
    const auto cal = toCalendarDate(tp);
    const int wd = weekdayOf(cal);
    if (wd >= 5) {
      tp -= Days{1};
      continue;
    }
    auto it = holidayCache.find(cal.year);
    if (it == holidayCache.end()) {
      it = holidayCache.emplace(cal.year, buildFederalHolidays(cal.year)).first;
    }
    if (it->second.contains(dayKey(cal))) {
      tp -= Days{1};
      continue;
    }
    return cal;
  }
}

} // namespace

// ----------------------------------------------------------------
// Key hashers
// ----------------------------------------------------------------

std::size_t
WindowCalendar::MonthlyKeyHash::operator()(const MonthlyKey &k) const noexcept {
  // Fold four small ints into a 64-bit value; they all fit in 16 bits.
  const auto v = (static_cast<std::uint64_t>(k.day) << 48U) |
                 (static_cast<std::uint64_t>(k.hour) << 32U) |
                 (static_cast<std::uint64_t>(k.minute) << 16U) |
                 static_cast<std::uint64_t>(k.second);
  return std::hash<std::uint64_t>{}(v);
}

std::size_t
WindowCalendar::AnnualKeyHash::operator()(const AnnualKey &k) const noexcept {
  const auto v = (static_cast<std::uint64_t>(k.month) << 56U) |
                 (static_cast<std::uint64_t>(k.day) << 48U) |
                 (static_cast<std::uint64_t>(k.hour) << 32U) |
                 (static_cast<std::uint64_t>(k.minute) << 16U) |
                 static_cast<std::uint64_t>(k.second);
  return std::hash<std::uint64_t>{}(v);
}

// ----------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------

WindowCalendar::WindowCalendar(TimePoint start, TimePoint endExcl)
    : start_(start), endExcl_(endExcl) {
  if (endExcl < start) {
    throw std::invalid_argument("WindowCalendar: endExcl must be >= start");
  }
  monthAnchors_ = monthStarts(start, endExcl);
}

// ----------------------------------------------------------------
// Slicing helper
// ----------------------------------------------------------------

std::span<const TimePoint>
WindowCalendar::slice(std::span<const TimePoint> items, TimePoint activeStart,
                      TimePoint activeEndExcl) const noexcept {
  const auto lo = std::lower_bound(items.begin(), items.end(), activeStart);
  const auto hi = std::lower_bound(lo, items.end(), activeEndExcl);
  return {lo, static_cast<std::size_t>(hi - lo)};
}

bool WindowCalendar::isBusinessDay(CalendarDate date) {
  if (weekdayOf(date) >= 5) {
    return false;
  }
  auto it = holidayCache_.find(date.year);
  if (it == holidayCache_.end()) {
    it =
        holidayCache_.emplace(date.year, buildFederalHolidays(date.year)).first;
  }
  return !it->second.contains(dayKey(date));
}

// ----------------------------------------------------------------
// iterMonthly
// ----------------------------------------------------------------

std::vector<TimePoint> WindowCalendar::iterMonthly(TimePoint activeStart,
                                                   TimePoint activeEndExcl,
                                                   int day, int hour,
                                                   int minute, int second) {
  if (day < 1 || day > 31) {
    throw std::invalid_argument("iterMonthly: day must be in [1, 31]");
  }

  const MonthlyKey key{day, hour, minute, second};

  auto it = monthlyCache_.find(key);
  if (it == monthlyCache_.end()) {
    std::vector<TimePoint> built;
    built.reserve(monthAnchors_.size());

    for (const auto anchor : monthAnchors_) {
      const auto cal = toCalendarDate(anchor);
      const auto dom = std::min<unsigned>(static_cast<unsigned>(day),
                                          daysInMonth(cal.year, cal.month));
      const auto ts = makeTime(CalendarDate{cal.year, cal.month, dom},
                               TimeOfDay{hour, minute, second});
      if (ts >= start_ && ts < endExcl_) {
        built.push_back(ts);
      }
    }

    it = monthlyCache_.emplace(key, std::move(built)).first;
  }

  const auto sliced = slice(it->second, activeStart, activeEndExcl);
  return std::vector<TimePoint>(sliced.begin(), sliced.end());
}

// ----------------------------------------------------------------
// iterAnnualFixed
// ----------------------------------------------------------------

std::vector<TimePoint> WindowCalendar::iterAnnualFixed(TimePoint activeStart,
                                                       TimePoint activeEndExcl,
                                                       int month, int day,
                                                       int hour, int minute,
                                                       int second) {
  if (month < 1 || month > 12) {
    throw std::invalid_argument("iterAnnualFixed: month must be in [1, 12]");
  }
  if (day < 1 || day > 31) {
    throw std::invalid_argument("iterAnnualFixed: day must be in [1, 31]");
  }

  const AnnualKey key{month, day, hour, minute, second};

  auto it = annualCache_.find(key);
  if (it == annualCache_.end()) {
    std::vector<TimePoint> built;

    const auto startCal = toCalendarDate(start_);
    const auto endCal = toCalendarDate(endExcl_);

    for (int year = startCal.year; year <= endCal.year; ++year) {
      const auto dom =
          std::min<unsigned>(static_cast<unsigned>(day),
                             daysInMonth(year, static_cast<unsigned>(month)));
      const auto ts =
          makeTime(CalendarDate{year, static_cast<unsigned>(month), dom},
                   TimeOfDay{hour, minute, second});
      if (ts >= start_ && ts < endExcl_) {
        built.push_back(ts);
      }
    }

    it = annualCache_.emplace(key, std::move(built)).first;
  }

  const auto sliced = slice(it->second, activeStart, activeEndExcl);
  return std::vector<TimePoint>(sliced.begin(), sliced.end());
}

// ----------------------------------------------------------------
// iterEstimatedTax
// ----------------------------------------------------------------

std::vector<TimePoint>
WindowCalendar::iterEstimatedTax(TimePoint activeStart,
                                 TimePoint activeEndExcl) {
  if (!quarterlyBuilt_) {
    const auto startCal = toCalendarDate(start_);
    const auto endCal = toCalendarDate(endExcl_);

    for (int year = startCal.year; year <= endCal.year; ++year) {
      for (const auto &md : kQuarterlyTaxDates) {
        const auto ts =
            makeTime(CalendarDate{year, static_cast<unsigned>(md.first),
                                  static_cast<unsigned>(md.second)},
                     TimeOfDay{10, 0, 0});
        if (ts >= start_ && ts < endExcl_) {
          quarterlyCache_.push_back(ts);
        }
      }
    }

    quarterlyBuilt_ = true;
  }

  const auto sliced = slice(quarterlyCache_, activeStart, activeEndExcl);
  return std::vector<TimePoint>(sliced.begin(), sliced.end());
}

// ----------------------------------------------------------------
// SSA payment dates
// ----------------------------------------------------------------

std::span<const TimePoint>
WindowCalendar::ssaPaymentDatesForBucket(int bucket) {
  if (bucket < 0 || bucket > 2) {
    throw std::invalid_argument(
        "ssaPaymentDatesForBucket: bucket must be in [0, 2]");
  }

  auto it = ssaCache_.find(bucket);
  if (it != ssaCache_.end()) {
    return {it->second.data(), it->second.size()};
  }

  const int birthDay = ssaBucketRepresentativeDay(bucket);

  std::vector<TimePoint> built;
  built.reserve(monthAnchors_.size());

  for (const auto anchor : monthAnchors_) {
    const auto cal = toCalendarDate(anchor);
    const auto payDate =
        ssaCyclePaymentDate(cal.year, cal.month, birthDay, holidayCache_);
    built.push_back(makeTime(payDate));
  }

  auto [inserted, _] = ssaCache_.emplace(bucket, std::move(built));
  return {inserted->second.data(), inserted->second.size()};
}

} // namespace PhantomLedger::time
