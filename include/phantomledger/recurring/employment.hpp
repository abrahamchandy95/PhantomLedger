#pragma once
/*
 * Employment state machine.
 *
 * Tracks the lifecycle of a person's job: employer assignment,
 * payroll cadence, salary with compounding raises, and job
 * transitions (switch to new employer with bump). Each person
 * has exactly one Employment state at any time.
 */

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/probability/distributions/cdf.hpp"
#include "phantomledger/recurring/growth.hpp"
#include "phantomledger/recurring/payroll.hpp"
#include "phantomledger/recurring/policy.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace PhantomLedger::recurring {

/// Mutable employment state for one person.
struct Employment {
  entity::Key employerAcct;
  PayrollProfile payroll;
  time::TimePoint start;
  time::TimePoint end;
  double annualSalary = 0.0;
  int switchIndex = 0;
};

/// Source callback for the initial annual salary draw.
using SalarySource = std::function<double()>;

/// Sample a payroll profile for an employer account.
[[nodiscard]] inline PayrollProfile
samplePayrollProfile(const PayrollPolicy &payrollPolicy,
                     const random::RngFactory &factory,
                     const entity::Key &employerAcct) {
  const auto key = std::to_string(employerAcct.number);
  auto rng = factory.rng({"employer_payroll_profile", key});

  const std::array<PayCadence, kPayCadenceCount> cadences = {
      PayCadence::weekly,
      PayCadence::biweekly,
      PayCadence::semimonthly,
      PayCadence::monthly,
  };

  const auto &weights = payrollPolicy.weights;
  const std::array<double, kPayCadenceCount> cadenceWeights = {
      weights.weekly,
      weights.biweekly,
      weights.semimonthly,
      weights.monthly,
  };

  const auto cdf = distributions::buildCdf(cadenceWeights);
  const auto cadence =
      cadences[distributions::sampleIndex(cdf, rng.nextDouble())];

  int weekday = payrollPolicy.defaultWeekday;
  if ((cadence == PayCadence::weekly || cadence == PayCadence::biweekly) &&
      rng.coin(0.25)) {
    weekday = (weekday == 4) ? 3 : 4;
  }

  auto anchor = nextWeekdayOnOrAfter(time::makeTime(time::CalendarDate{
                                         .year = 2025,
                                         .month = 1,
                                         .day = 1,
                                     }),
                                     weekday);
  if (cadence == PayCadence::biweekly && rng.coin(0.5)) {
    anchor = time::addDays(anchor, 7);
  }

  const int lagMax = std::max(0, payrollPolicy.postingLagDaysMax);
  const int lag =
      (lagMax == 0) ? 0 : static_cast<int>(rng.uniformInt(0, lagMax + 1));

  PayrollProfile profile;
  profile.cadence = cadence;
  profile.anchorDate = anchor;
  profile.weekday = weekday;
  profile.postingLagDays = lag;

  if (cadence == PayCadence::semimonthly) {
    profile.semimonthlyDays =
        rng.coin(0.35) ? std::array<int, 2>{1, 15} : std::array<int, 2>{15, 31};
  }

  if (cadence == PayCadence::monthly) {
    const std::array<int, 3> choices = {28, 30, 31};
    profile.monthlyDay = choices[rng.choiceIndex(choices.size())];
  }

  return profile;
}

class SalaryGrowthModel {
public:
  SalaryGrowthModel(const InflationPolicy &inflationPolicy,
                    const RaisePolicy &raisePolicy,
                    const random::RngFactory &factory)
      : inflationPolicy_(inflationPolicy), raisePolicy_(raisePolicy),
        factory_(factory) {}

  [[nodiscard]] double compound(std::string_view personId,
                                time::TimePoint start,
                                time::TimePoint now) const {
    const int years = growth::anniversariesPassed(start, now);
    if (years <= 0) {
      return 1.0;
    }

    const auto startCal = time::toCalendarDate(start);
    double growthFactor = 1.0;

    for (int i = 0; i < years; ++i) {
      const double realRaise = salaryRealRaise(personId, startCal.year + i);
      growthFactor *= (1.0 + inflationPolicy_.annual + realRaise);
    }

    return growthFactor;
  }

  [[nodiscard]] double jobSwitchBump(std::string_view personId,
                                     int switchIndex) const {
    auto rng = factory_.rng(
        {"job_switch_bump", personId, std::to_string(switchIndex)});
    return growth::sampleNormalClamped(rng, raisePolicy_.jobBump.mu,
                                       raisePolicy_.jobBump.sigma,
                                       raisePolicy_.jobBump.floor);
  }

private:
  [[nodiscard]] double salaryRealRaise(std::string_view personId,
                                       int year) const {
    auto rng =
        factory_.rng({"salary_real_raise", personId, std::to_string(year)});
    return growth::sampleNormalClamped(rng, raisePolicy_.salary.mu,
                                       raisePolicy_.salary.sigma,
                                       raisePolicy_.salary.floor);
  }

  const InflationPolicy &inflationPolicy_;
  const RaisePolicy &raisePolicy_;
  const random::RngFactory &factory_;
};

class EmploymentInitializer {
public:
  EmploymentInitializer(const TenurePolicy &tenurePolicy,
                        const PayrollPolicy &payrollPolicy,
                        const random::RngFactory &factory)
      : tenurePolicy_(tenurePolicy), payrollPolicy_(payrollPolicy),
        factory_(factory) {}

  [[nodiscard]] Employment operator()(std::string_view personId,
                                      time::TimePoint startDate,
                                      std::span<const entity::Key> employers,
                                      const SalarySource &salarySource) const {
    auto rng = factory_.rng({"employment_init", personId});

    const auto employer = pickInitialEmployer(rng, employers);
    const auto payroll =
        samplePayrollProfile(payrollPolicy_, factory_, employer);
    const auto interval =
        sampleInitialJobInterval(rng, startDate, tenurePolicy_.job);
    const double annualSalary = sampleInitialAnnualSalary(rng, salarySource);

    return Employment{
        .employerAcct = employer,
        .payroll = payroll,
        .start = interval.start,
        .end = interval.end,
        .annualSalary = annualSalary,
        .switchIndex = 0,
    };
  }

private:
  struct JobInterval {
    time::TimePoint start;
    time::TimePoint end;
  };

  [[nodiscard]] static entity::Key
  pickInitialEmployer(random::Rng &rng,
                      std::span<const entity::Key> employers) {
    return growth::pickOne(rng, employers);
  }

  [[nodiscard]] static JobInterval
  sampleInitialJobInterval(random::Rng &rng, time::TimePoint startDate,
                           const Range &jobTenure) {
    const auto [start, end] = growth::sampleBackdatedInterval(
        rng, startDate, jobTenure.min, jobTenure.max);
    return JobInterval{.start = start, .end = end};
  }

  [[nodiscard]] static double
  sampleInitialAnnualSalary(random::Rng &rng,
                            const SalarySource &salarySource) {
    return salarySource() * growth::sampleLognormalMultiplier(rng, 0.03);
  }

  const TenurePolicy &tenurePolicy_;
  const PayrollPolicy &payrollPolicy_;
  const random::RngFactory &factory_;
};

class EmploymentAdvancer {
public:
  EmploymentAdvancer(const TenurePolicy &tenurePolicy,
                     const PayrollPolicy &payrollPolicy,
                     const SalaryGrowthModel &salaryGrowth,
                     const random::RngFactory &factory)
      : tenurePolicy_(tenurePolicy), payrollPolicy_(payrollPolicy),
        salaryGrowth_(salaryGrowth), factory_(factory) {}

  [[nodiscard]] Employment operator()(random::Rng &rng,
                                      std::string_view personId,
                                      time::TimePoint now,
                                      std::span<const entity::Key> employers,
                                      const Employment &previous) const {
    const auto employer =
        growth::pickDifferent(rng, employers, previous.employerAcct);
    const auto payroll =
        samplePayrollProfile(payrollPolicy_, factory_, employer);
    const auto [start, end] = growth::sampleForwardInterval(
        rng, now, tenurePolicy_.job.min, tenurePolicy_.job.max);

    const double currentSalary =
        previous.annualSalary *
        salaryGrowth_.compound(personId, previous.start, now);

    const double bump =
        salaryGrowth_.jobSwitchBump(personId, previous.switchIndex + 1);

    return Employment{
        .employerAcct = employer,
        .payroll = payroll,
        .start = start,
        .end = end,
        .annualSalary = currentSalary * (1.0 + bump),
        .switchIndex = previous.switchIndex + 1,
    };
  }

private:
  const TenurePolicy &tenurePolicy_;
  const PayrollPolicy &payrollPolicy_;
  const SalaryGrowthModel &salaryGrowth_;
  const random::RngFactory &factory_;
};

class SalaryCalculator {
public:
  explicit SalaryCalculator(const SalaryGrowthModel &salaryGrowth)
      : salaryGrowth_(salaryGrowth) {}

  [[nodiscard]] double operator()(std::string_view personId,
                                  const Employment &state,
                                  time::TimePoint payDate) const {
    const double annual =
        state.annualSalary *
        salaryGrowth_.compound(personId, state.start, payDate);

    const auto cal = time::toCalendarDate(payDate);
    const int periods = payPeriodsInYear(state.payroll, cal.year);

    return primitives::utils::roundMoney(
        std::max(50.0, annual / static_cast<double>(periods)));
  }

private:
  const SalaryGrowthModel &salaryGrowth_;
};

/// Get pay dates within the employment's active window intersected with
/// the simulation window.
[[nodiscard]] inline std::vector<time::TimePoint>
paydatesForWindow(const Employment &state, time::TimePoint windowStart,
                  time::TimePoint windowEndExcl) {
  const auto activeStart = std::max(windowStart, state.start);
  const auto activeEnd = std::min(windowEndExcl, state.end);

  if (activeEnd <= activeStart) {
    return {};
  }

  return paydatesForProfile(state.payroll, activeStart, activeEnd);
}

} // namespace PhantomLedger::recurring
