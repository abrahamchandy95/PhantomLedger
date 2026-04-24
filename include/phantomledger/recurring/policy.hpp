#pragma once
/*
 * Recurring relationship policy parameters.
 *
 * Composes smaller policy types so each one owns a single concern:
 * activation, tenure, inflation, raises, and payroll behavior.
 */
#include "phantomledger/primitives/validate/checks.hpp"

namespace PhantomLedger::recurring {

namespace detail {

[[nodiscard]] constexpr bool unit(double value) noexcept {
  return value >= 0.0 && value <= 1.0;
}

[[nodiscard]] constexpr bool nonNegative(double value) noexcept {
  return value >= 0.0;
}

[[nodiscard]] constexpr bool orderedRange(double minValue,
                                          double maxValue) noexcept {
  return minValue >= 0.0 && maxValue >= minValue;
}

} // namespace detail

struct Range {
  double min = 0.0;
  double max = 0.0;

  [[nodiscard]] constexpr bool valid() const noexcept {
    return detail::orderedRange(min, max);
  }
};

struct GrowthDist {
  double mu = 0.0;
  double sigma = 0.0;
  double floor = 0.0;

  [[nodiscard]] constexpr bool valid() const noexcept {
    return detail::nonNegative(sigma);
  }
};

struct ActivationPolicy {
  double salary = 0.65;
  double rent = 0.55;

  [[nodiscard]] constexpr bool valid() const noexcept {
    return detail::unit(salary) && detail::unit(rent);
  }
};

struct TenurePolicy {
  Range job{2.0, 10.0};
  Range lease{2.0, 10.0};

  [[nodiscard]] constexpr bool valid() const noexcept {
    return job.valid() && lease.valid();
  }
};

struct InflationPolicy {
  double annual = 0.03;

  [[nodiscard]] constexpr bool valid() const noexcept { return annual > -1.0; }
};

struct RaisePolicy {
  GrowthDist salary{0.02, 0.01, 0.005};
  GrowthDist jobBump{0.08, 0.05, 0.00};
  GrowthDist rent{0.03, 0.02, 0.00};

  [[nodiscard]] constexpr bool valid() const noexcept {
    return salary.valid() && jobBump.valid() && rent.valid();
  }
};

struct PayrollWeights {
  double weekly = 0.27;
  double biweekly = 0.43;
  double semimonthly = 0.20;
  double monthly = 0.10;

  [[nodiscard]] constexpr double total() const noexcept {
    return weekly + biweekly + semimonthly + monthly;
  }

  [[nodiscard]] constexpr bool valid() const noexcept {
    return detail::nonNegative(weekly) && detail::nonNegative(biweekly) &&
           detail::nonNegative(semimonthly) && detail::nonNegative(monthly) &&
           total() > 0.0;
  }

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::nonNegative("weekly", weekly); });
    r.check([&] { v::nonNegative("biweekly", biweekly); });
    r.check([&] { v::nonNegative("semimonthly", semimonthly); });
    r.check([&] { v::nonNegative("monthly", monthly); });
    r.check([&] { v::gt("total", total(), 0.0); });
  }
};

struct PayrollPolicy {
  PayrollWeights weights{};
  int defaultWeekday = 4; // Friday (Mon=0)
  int postingLagDaysMax = 1;

  [[nodiscard]] constexpr bool valid() const noexcept {
    return weights.valid() && defaultWeekday >= 0 && defaultWeekday <= 6 &&
           postingLagDaysMax >= 0;
  }
};

struct Policy {
  ActivationPolicy activation{};
  TenurePolicy tenure{};
  InflationPolicy inflation{};
  RaisePolicy raises{};
  PayrollPolicy payroll{};

  [[nodiscard]] constexpr bool valid() const noexcept {
    return activation.valid() && tenure.valid() && inflation.valid() &&
           raises.valid() && payroll.valid();
  }
};

inline constexpr Policy kDefaultPolicy{};

} // namespace PhantomLedger::recurring
