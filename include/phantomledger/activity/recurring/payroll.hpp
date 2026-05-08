#pragma once

#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/taxonomies/recurring/types.hpp"

#include <algorithm>
#include <array>
#include <vector>

namespace PhantomLedger::recurring {

struct PayrollWeights {
  double weekly = 0.20;
  double biweekly = 0.55;
  double semimonthly = 0.15;
  double monthly = 0.10;

  [[nodiscard]] constexpr double total() const noexcept {
    return weekly + biweekly + semimonthly + monthly;
  }

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;

    r.check([&] { v::finite("weekly", weekly); });
    r.check([&] { v::finite("biweekly", biweekly); });
    r.check([&] { v::finite("semimonthly", semimonthly); });
    r.check([&] { v::finite("monthly", monthly); });

    r.check([&] { v::nonNegative("weekly", weekly); });
    r.check([&] { v::nonNegative("biweekly", biweekly); });
    r.check([&] { v::nonNegative("semimonthly", semimonthly); });
    r.check([&] { v::nonNegative("monthly", monthly); });

    r.check([&] { v::gt("total", total(), 0.0); });
  }
};

struct PayrollRules {
  PayrollWeights weights{};
  int defaultWeekday = 4;
  int postingLagDaysMax = 1;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;

    weights.validate(r);

    r.check([&] { v::between("defaultWeekday", defaultWeekday, 0, 6); });
    r.check([&] { v::nonNegative("postingLagDaysMax", postingLagDaysMax); });
  }
};

struct PayrollProfile {
  PayCadence cadence = PayCadence::biweekly;
  time::TimePoint anchorDate{};
  int weekday = 4;

  // Semimonthly only.
  std::array<int, 2> semimonthlyDays = {15, 31};

  // Monthly only.
  int monthlyDay = 31;

  WeekendRoll weekendRoll = WeekendRoll::previousBusinessDay;
  int postingLagDays = 0;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;

    r.check([&] { v::between("weekday", weekday, 0, 6); });

    r.check(
        [&] { v::between("semimonthlyDays[0]", semimonthlyDays[0], 1, 31); });
    r.check(
        [&] { v::between("semimonthlyDays[1]", semimonthlyDays[1], 1, 31); });

    r.check([&] {
      const auto first = std::min(semimonthlyDays[0], semimonthlyDays[1]);
      const auto second = std::max(semimonthlyDays[0], semimonthlyDays[1]);
      v::gt("semimonthlyDays[1]", second, first);
    });

    r.check([&] { v::between("monthlyDay", monthlyDay, 1, 31); });
    r.check([&] { v::nonNegative("postingLagDays", postingLagDays); });
  }
};

[[nodiscard]] inline time::TimePoint rollWeekend(time::TimePoint ts,
                                                 WeekendRoll mode) {
  if (mode == WeekendRoll::none) {
    return ts;
  }

  const int step = (mode == WeekendRoll::previousBusinessDay) ? -1 : 1;
  while (time::isWeekend(ts)) {
    ts = time::addDays(ts, step);
  }
  return ts;
}

[[nodiscard]] inline time::TimePoint applyPostingLag(time::TimePoint ts,
                                                     int postingLagDays) {
  if (postingLagDays <= 0) {
    return ts;
  }

  return time::addDays(ts, postingLagDays);
}

[[nodiscard]] inline time::TimePoint
finalizePayDate(time::TimePoint ts, const PayrollProfile &profile) {
  ts = rollWeekend(ts, profile.weekendRoll);
  ts = applyPostingLag(ts, profile.postingLagDays);
  return rollWeekend(ts, profile.weekendRoll);
}

/// Find the next occurrence of a given weekday on or after ts.
[[nodiscard]] inline time::TimePoint nextWeekdayOnOrAfter(time::TimePoint ts,
                                                          int weekday) {
  primitives::validate::between("weekday", weekday, 0, 6);

  const auto cal = time::toCalendarDate(ts);

  const auto midnight = time::makeTime(time::CalendarDate{
      .year = cal.year,
      .month = cal.month,
      .day = cal.day,
  });

  const int delta = (weekday - time::weekday(midnight) + 7) % 7;
  return time::addDays(midnight, delta);
}

[[nodiscard]] inline std::array<int, 2>
sortedSemimonthlyDays(const PayrollProfile &profile) {
  auto days = profile.semimonthlyDays;

  if (days[0] > days[1]) {
    std::swap(days[0], days[1]);
  }

  return days;
}

namespace detail {

inline void advanceMonth(int &year, unsigned &month) noexcept {
  if (month == 12) {
    ++year;
    month = 1;
    return;
  }

  ++month;
}

[[nodiscard]] inline time::TimePoint monthDate(int year, unsigned month,
                                               int day) {
  const unsigned clampedDay =
      std::min(static_cast<unsigned>(day), time::daysInMonth(year, month));

  return time::makeTime(time::CalendarDate{
      .year = year,
      .month = month,
      .day = clampedDay,
  });
}

inline void appendIfInWindow(std::vector<time::TimePoint> &out,
                             time::TimePoint payDate, time::TimePoint start,
                             time::TimePoint endExcl) {
  if (payDate >= start && payDate < endExcl) {
    out.push_back(payDate);
  }
}

} // namespace detail

/// Iterate all scheduled pay dates within [start, endExcl).
[[nodiscard]] inline std::vector<time::TimePoint>
paydatesForProfile(const PayrollProfile &profile, time::TimePoint start,
                   time::TimePoint endExcl) {
  std::vector<time::TimePoint> out;

  if (endExcl <= start) {
    return out;
  }

  primitives::validate::require(profile);

  const auto startCal = time::toCalendarDate(start);

  switch (profile.cadence) {
  case PayCadence::weekly: {
    auto current = nextWeekdayOnOrAfter(std::max(start, profile.anchorDate),
                                        profile.weekday);

    while (current < endExcl) {
      detail::appendIfInWindow(out, finalizePayDate(current, profile), start,
                               endExcl);
      current = time::addDays(current, 7);
    }

    break;
  }

  case PayCadence::biweekly: {
    auto current = nextWeekdayOnOrAfter(profile.anchorDate, profile.weekday);

    while (current < start) {
      current = time::addDays(current, 14);
    }

    while (current < endExcl) {
      detail::appendIfInWindow(out, finalizePayDate(current, profile), start,
                               endExcl);
      current = time::addDays(current, 14);
    }

    break;
  }

  case PayCadence::semimonthly: {
    int year = startCal.year;
    unsigned month = startCal.month;
    const auto days = sortedSemimonthlyDays(profile);

    while (true) {
      bool done = false;

      for (int day : days) {
        const auto payDate =
            finalizePayDate(detail::monthDate(year, month, day), profile);

        if (payDate >= endExcl) {
          done = true;
          break;
        }

        detail::appendIfInWindow(out, payDate, start, endExcl);
      }

      if (done) {
        break;
      }

      detail::advanceMonth(year, month);
    }

    break;
  }

  case PayCadence::monthly: {
    int year = startCal.year;
    unsigned month = startCal.month;

    while (true) {
      const auto payDate = finalizePayDate(
          detail::monthDate(year, month, profile.monthlyDay), profile);

      if (payDate >= endExcl) {
        break;
      }

      detail::appendIfInWindow(out, payDate, start, endExcl);
      detail::advanceMonth(year, month);
    }

    break;
  }
  }

  return out;
}

/// Count pay periods in a calendar year.
[[nodiscard]] inline int payPeriodsInYear(const PayrollProfile &profile,
                                          int year) {
  primitives::validate::require(profile);

  if (profile.cadence == PayCadence::semimonthly) {
    return 24;
  }

  if (profile.cadence == PayCadence::monthly) {
    return 12;
  }

  const auto start = time::makeTime(time::CalendarDate{
      .year = year,
      .month = 1,
      .day = 1,
  });

  const auto end = time::makeTime(time::CalendarDate{
      .year = year + 1,
      .month = 1,
      .day = 1,
  });

  const auto dates = paydatesForProfile(profile, start, end);
  return std::max(1, static_cast<int>(dates.size()));
}

} // namespace PhantomLedger::recurring
