#pragma once
/*
 * WindowCalendar — cached date iteration over a simulation window.
 *
 * Every recurring event in the simulation (monthly premiums, annual
 * taxes, SSA cycle payments, quarterly estimated tax, etc.) needs to
 * enumerate the set of timestamps on which it fires. Naively each
 * generator would re-derive the same sequences; WindowCalendar caches
 * them behind a small key-based map.
 *
 * Key design choices:
 *
 *   - All iteration functions are half-open: [activeStart, activeEndExcl).
 *     This matches the convention used everywhere else in the ledger and
 *     avoids off-by-one errors at month boundaries.
 *
 *   - iterMonthly clamps the day-of-month to the last day of the given
 *     month. Requesting day=31 on a month with 30 days silently yields
 *     day=30; requesting day=29 in February of a non-leap year yields
 *     day=28. This mirrors the Python behavior.
 *
 *   - SSA cohorting follows the post-1997 rule: cohort 0 (birth day
 *     1-10) pays on the 2nd Wednesday, cohort 1 (11-20) on the 3rd,
 *     cohort 2 (21-31) on the 4th. If the scheduled Wednesday is a
 *     federal holiday, SSA pays on the prior business day.
 *
 *   - The holiday cache is built lazily by year and shared across all
 *     SSA cohort lookups for the same WindowCalendar instance.
 */

#include "phantomledger/primitives/time/calendar.hpp"

#include <cstdint>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace PhantomLedger::time {

/// Which SSA cycle bucket a given day-of-birth falls into.
///
///   birthDay in [1, 10]  -> 0 (2nd Wednesday)
///   birthDay in [11, 20] -> 1 (3rd Wednesday)
///   birthDay in [21, 31] -> 2 (4th Wednesday)
[[nodiscard]] constexpr int ssaBucketForBirthDay(int birthDay) noexcept {
  if (birthDay <= 10) {
    return 0;
  }
  if (birthDay <= 20) {
    return 1;
  }
  return 2;
}

/// Representative day-of-month for each SSA bucket, used when the
/// generator only knows the bucket and wants a stable pay date.
[[nodiscard]] constexpr int ssaBucketRepresentativeDay(int bucket) noexcept {
  switch (bucket) {
  case 0:
    return 1;
  case 1:
    return 11;
  case 2:
    return 21;
  default:
    return 1;
  }
}

class WindowCalendar {
public:
  /// Constructs a calendar covering [start, endExcl). Throws
  /// std::invalid_argument if endExcl < start.
  WindowCalendar(TimePoint start, TimePoint endExcl);

  [[nodiscard]] TimePoint start() const noexcept { return start_; }
  [[nodiscard]] TimePoint endExcl() const noexcept { return endExcl_; }

  /// Midnight on the first day of every month that overlaps the window.
  /// Cached and immutable after construction.
  [[nodiscard]] std::span<const TimePoint> monthAnchors() const noexcept {
    return {monthAnchors_.data(), monthAnchors_.size()};
  }

  // --- Monthly cadence ---

  /// Every occurrence of (day-of-month, hour, minute, second) inside
  /// the window, clipped to [activeStart, activeEndExcl).
  ///
  /// `day` is clamped to the month's last day; values outside [1, 31]
  /// trigger std::invalid_argument.
  [[nodiscard]] std::vector<TimePoint>
  iterMonthly(TimePoint activeStart, TimePoint activeEndExcl, int day,
              int hour = 0, int minute = 0, int second = 0);

  // --- Annual fixed date ---

  /// Every occurrence of (month, day-of-month, time) inside the window,
  /// clipped to [activeStart, activeEndExcl).
  [[nodiscard]] std::vector<TimePoint>
  iterAnnualFixed(TimePoint activeStart, TimePoint activeEndExcl, int month,
                  int day, int hour = 0, int minute = 0, int second = 0);

  // --- Estimated quarterly tax (IRS 1040-ES) ---
  //
  // Fixed dates: Jan 15 (prior-year Q4), April 15 (Q1), June 15 (Q2),
  // September 15 (Q3). Filed at 10:00 local per convention.
  [[nodiscard]] std::vector<TimePoint>
  iterEstimatedTax(TimePoint activeStart, TimePoint activeEndExcl);

  // --- SSA cycle payment dates ---

  /// Calendar dates (midnight TimePoints) on which an SSA cohort is
  /// paid across the window. `bucket` must be in [0, 2].
  [[nodiscard]] std::span<const TimePoint> ssaPaymentDatesForBucket(int bucket);

  /// Convenience overload — resolves bucket from birth day first.
  [[nodiscard]] std::span<const TimePoint> ssaPaymentDates(int birthDay) {
    return ssaPaymentDatesForBucket(ssaBucketForBirthDay(birthDay));
  }

private:
  struct MonthlyKey {
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    [[nodiscard]] bool operator==(const MonthlyKey &) const noexcept = default;
  };

  struct MonthlyKeyHash {
    [[nodiscard]] std::size_t operator()(const MonthlyKey &k) const noexcept;
  };

  struct AnnualKey {
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    [[nodiscard]] bool operator==(const AnnualKey &) const noexcept = default;
  };

  struct AnnualKeyHash {
    [[nodiscard]] std::size_t operator()(const AnnualKey &k) const noexcept;
  };

  TimePoint start_;
  TimePoint endExcl_;
  std::vector<TimePoint> monthAnchors_;

  std::unordered_map<MonthlyKey, std::vector<TimePoint>, MonthlyKeyHash>
      monthlyCache_;
  std::unordered_map<AnnualKey, std::vector<TimePoint>, AnnualKeyHash>
      annualCache_;
  std::vector<TimePoint> quarterlyCache_;
  bool quarterlyBuilt_ = false;
  std::unordered_map<int, std::vector<TimePoint>> ssaCache_;

  // Per-year set of federal holidays, lazily built.
  std::unordered_map<int, std::unordered_set<std::int64_t>> holidayCache_;

  [[nodiscard]] std::span<const TimePoint>
  slice(std::span<const TimePoint> items, TimePoint activeStart,
        TimePoint activeEndExcl) const noexcept;

  [[nodiscard]] bool isBusinessDay(CalendarDate date);
};

} // namespace PhantomLedger::time
