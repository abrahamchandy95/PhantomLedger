#pragma once
/*
 * Almanac — memoized index of recurring date sequences over a
 * simulation window.
 *
 * Every recurring event in the simulation (monthly premiums, annual
 * taxes, SSA cycle payments, quarterly estimated tax, etc.) needs to
 * enumerate the set of timestamps on which it fires. Naively each
 * emitter would re-derive the same sequences; the almanac caches
 * them behind a small key-based map.
 */

#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"

#include <cstdint>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace PhantomLedger::time {

/// SSA payment cohort for a given day-of-birth.
[[nodiscard]] constexpr int ssaCohort(int birthDay) noexcept {
  if (birthDay <= 10) {
    return 0;
  }
  if (birthDay <= 20) {
    return 1;
  }
  return 2;
}

/// Anchor day-of-month for an SSA cohort, used when the caller has
[[nodiscard]] constexpr int ssaCohortAnchorDay(int cohort) noexcept {
  switch (cohort) {
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

class Almanac {
public:
  /// Constructs an almanac covering [start, endExcl)
  Almanac(TimePoint start, TimePoint endExcl);

  explicit Almanac(const Window &window)
      : Almanac(window.start, window.endExcl()) {}

  [[nodiscard]] TimePoint start() const noexcept { return start_; }
  [[nodiscard]] TimePoint endExcl() const noexcept { return endExcl_; }

  /// Midnight on the first day of every month overlapping the window.
  [[nodiscard]] std::span<const TimePoint> monthAnchors() const noexcept {
    return {monthAnchors_.data(), monthAnchors_.size()};
  }

  [[nodiscard]] std::vector<TimePoint> monthly(TimePoint activeStart,
                                               TimePoint activeEndExcl, int day,
                                               int hour = 0, int minute = 0,
                                               int second = 0);

  /// Every (month, day-of-month, time) inside the window, clipped to
  /// [activeStart, activeEndExcl).
  [[nodiscard]] std::vector<TimePoint> annual(TimePoint activeStart,
                                              TimePoint activeEndExcl,
                                              int month, int day, int hour = 0,
                                              int minute = 0, int second = 0);

  /// IRS 1040-ES estimated tax due dates: Jan 15 (prior-year Q4),
  /// April 15 (Q1), June 15 (Q2), September 15 (Q3). Filed at 10:00.
  [[nodiscard]] std::vector<TimePoint> estimatedTax(TimePoint activeStart,
                                                    TimePoint activeEndExcl);

  /// Pay dates for one SSA cohort across the window. `cohort` must
  /// be in [0, 2].
  [[nodiscard]] std::span<const TimePoint> ssaPayDates(int cohort);

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

  std::unordered_map<int, std::unordered_set<std::int64_t>> holidayCache_;

  [[nodiscard]] std::span<const TimePoint>
  slice(std::span<const TimePoint> items, TimePoint activeStart,
        TimePoint activeEndExcl) const noexcept;

  [[nodiscard]] bool isBusinessDay(CalendarDate date);
};

} // namespace PhantomLedger::time
