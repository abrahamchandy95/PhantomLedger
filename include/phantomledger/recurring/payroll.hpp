#pragma once
/*
 * Payroll profile and pay cadence.
 *
 * Defines how an employer pays its employees: weekly, biweekly,
 * semimonthly, or monthly, along with anchor dates, weekend roll
 * conventions, and posting lag.
 */

#include "phantomledger/time/calendar.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace PhantomLedger::recurring {

enum class PayCadence : std::uint8_t {
  weekly = 0,
  biweekly = 1,
  semimonthly = 2,
  monthly = 3,
};

inline constexpr std::size_t kPayCadenceCount = 4;

enum class WeekendRoll : std::uint8_t {
  previousBusinessDay = 0,
  nextBusinessDay = 1,
  none = 2,
};

struct PayrollProfile {
  PayCadence cadence = PayCadence::biweekly;
  time::TimePoint anchorDate{};
  int weekday = 4; // Friday (Mon=0)

  // Semimonthly only.
  std::array<int, 2> semimonthlyDays = {15, 31};

  // Monthly only.
  int monthlyDay = 31;

  WeekendRoll weekendRoll = WeekendRoll::previousBusinessDay;
  int postingLagDays = 0;

  [[nodiscard]] constexpr bool valid() const noexcept {
    return weekday >= 0 && weekday <= 6 && semimonthlyDays[0] >= 1 &&
           semimonthlyDays[1] >= 1 && monthlyDay >= 1 && postingLagDays >= 0;
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
  ts = rollWeekend(ts, profile.weekendRoll);
  return ts;
}

/// Find the next occurrence of a given weekday on or after ts.
[[nodiscard]] inline time::TimePoint nextWeekdayOnOrAfter(time::TimePoint ts,
                                                          int weekday) {
  const auto cal = time::toCalendarDate(ts);
  auto midnight = time::makeTime(cal.year, cal.month, cal.day);
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

/// Iterate all scheduled pay dates within [start, endExcl).
[[nodiscard]] inline std::vector<time::TimePoint>
paydatesForProfile(const PayrollProfile &profile, time::TimePoint start,
                   time::TimePoint endExcl) {
  std::vector<time::TimePoint> out;
  if (endExcl <= start || !profile.valid()) {
    return out;
  }

  const auto startCal = time::toCalendarDate(start);

  switch (profile.cadence) {
  case PayCadence::weekly: {
    auto current = nextWeekdayOnOrAfter(std::max(start, profile.anchorDate),
                                        profile.weekday);

    while (current < endExcl) {
      const auto payDate = finalizePayDate(current, profile);
      if (payDate >= start && payDate < endExcl) {
        out.push_back(payDate);
      }
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
      const auto payDate = finalizePayDate(current, profile);
      if (payDate >= start && payDate < endExcl) {
        out.push_back(payDate);
      }
      current = time::addDays(current, 14);
    }
    break;
  }

  case PayCadence::semimonthly: {
    int year = startCal.year;
    auto month = startCal.month;
    const auto days = sortedSemimonthlyDays(profile);

    while (true) {
      bool done = false;

      for (int day : days) {
        const unsigned clampedDay = std::min(static_cast<unsigned>(day),
                                             time::daysInMonth(year, month));

        const auto payDate =
            finalizePayDate(time::makeTime(year, month, clampedDay), profile);

        if (payDate >= endExcl) {
          done = true;
          break;
        }

        if (payDate >= start) {
          out.push_back(payDate);
        }
      }

      if (done) {
        break;
      }

      if (month == 12) {
        ++year;
        month = 1;
      } else {
        ++month;
      }
    }
    break;
  }

  case PayCadence::monthly: {
    int year = startCal.year;
    auto month = startCal.month;

    while (true) {
      const unsigned clampedDay =
          std::min(static_cast<unsigned>(profile.monthlyDay),
                   time::daysInMonth(year, month));

      const auto payDate =
          finalizePayDate(time::makeTime(year, month, clampedDay), profile);

      if (payDate >= endExcl) {
        break;
      }

      if (payDate >= start) {
        out.push_back(payDate);
      }

      if (month == 12) {
        ++year;
        month = 1;
      } else {
        ++month;
      }
    }
    break;
  }
  }

  return out;
}

/// Count pay periods in a calendar year.
[[nodiscard]] inline int payPeriodsInYear(const PayrollProfile &profile,
                                          int year) {
  if (profile.cadence == PayCadence::semimonthly) {
    return 24;
  }
  if (profile.cadence == PayCadence::monthly) {
    return 12;
  }

  const auto start = time::makeTime(year, 1, 1);
  const auto end = time::makeTime(year + 1, 1, 1);
  const auto dates = paydatesForProfile(profile, start, end);
  return std::max(1, static_cast<int>(dates.size()));
}

} // namespace PhantomLedger::recurring
