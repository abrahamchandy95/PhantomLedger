#pragma once
/*
 * Employment state machine.
 *
 * Tracks the lifecycle of a person's job: employer assignment,
 * payroll cadence, salary with compounding raises, and job
 * transitions (switch to new employer with bump). Each person
 * has exactly one Employment state at any time.
 */

#include "phantomledger/distributions/cdf.hpp"
#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/random/factory.hpp"
#include "phantomledger/random/rng.hpp"
#include "phantomledger/recurring/growth.hpp"
#include "phantomledger/recurring/payroll.hpp"
#include "phantomledger/recurring/policy.hpp"
#include "phantomledger/time/calendar.hpp"

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
  entities::identifier::Key employerAcct;
  PayrollProfile payroll;
  time::TimePoint start;
  time::TimePoint end;
  double annualSalary = 0.0;
  int switchIndex = 0;
};

/// Source callback for the initial annual salary draw.
using SalarySource = std::function<double()>;

/// Narrow policy view used by employment logic.
struct EmploymentRules {
  const TenurePolicy &tenure;
  const InflationPolicy &inflation;
  const RaisePolicy &raises;
  const PayrollPolicy &payroll;
};

struct EmploymentInitInput {
  std::string_view personId;
  time::TimePoint startDate;
  std::span<const entities::identifier::Key> employers;
  SalarySource salarySource;
};

struct EmploymentAdvanceInput {
  std::string_view personId;
  time::TimePoint now;
  std::span<const entities::identifier::Key> employers;
  const Employment &previous;
};

struct SalaryQuery {
  std::string_view personId;
  const Employment &state;
  time::TimePoint payDate;
};

namespace detail {

struct SalaryGrowthInput {
  std::string_view personId;
  time::TimePoint start;
  time::TimePoint now;
};

[[nodiscard]] inline double salaryRealRaise(const random::RngFactory &factory,
                                            const RaisePolicy &raises,
                                            std::string_view key, int year) {
  auto rng = factory.rng({"salary_real_raise", key, std::to_string(year)});
  return growth::sampleNormalClamped(rng, raises.salary.mu, raises.salary.sigma,
                                     raises.salary.floor);
}

[[nodiscard]] inline double jobSwitchBump(const random::RngFactory &factory,
                                          const RaisePolicy &raises,
                                          std::string_view personId,
                                          int switchIndex) {
  auto rng =
      factory.rng({"job_switch_bump", personId, std::to_string(switchIndex)});
  return growth::sampleNormalClamped(
      rng, raises.jobBump.mu, raises.jobBump.sigma, raises.jobBump.floor);
}

[[nodiscard]] inline double compoundSalary(const EmploymentRules &rules,
                                           const random::RngFactory &factory,
                                           const SalaryGrowthInput &input) {
  const int years = growth::anniversariesPassed(input.start, input.now);
  if (years <= 0) {
    return 1.0;
  }

  const auto startCal = time::toCalendarDate(input.start);
  double growthFactor = 1.0;

  for (int i = 0; i < years; ++i) {
    const double realRaise = salaryRealRaise(factory, rules.raises,
                                             input.personId, startCal.year + i);
    growthFactor *= (1.0 + rules.inflation.annual + realRaise);
  }

  return growthFactor;
}

} // namespace detail

/// Sample a payroll profile for an employer account.
[[nodiscard]] inline PayrollProfile
samplePayrollProfile(const PayrollPolicy &payrollPolicy,
                     const random::RngFactory &factory,
                     const entities::identifier::Key &employerAcct) {
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
  const auto cadence = cadences[rng.weightedIndex(cdf)];

  int weekday = payrollPolicy.defaultWeekday;
  if ((cadence == PayCadence::weekly || cadence == PayCadence::biweekly) &&
      rng.coin(0.25)) {
    weekday = (weekday == 4) ? 3 : 4;
  }

  auto anchor = nextWeekdayOnOrAfter(time::makeTime(2025, 1, 1), weekday);
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

/// Bootstrap the initial employment state for a person.
[[nodiscard]] inline Employment
initializeEmployment(const EmploymentRules &rules,
                     const random::RngFactory &factory,
                     const EmploymentInitInput &input) {
  auto rng = factory.rng({"employment_init", input.personId});

  const auto employer = growth::pickOne(rng, input.employers);
  const auto payroll = samplePayrollProfile(rules.payroll, factory, employer);
  const auto [jobStart, jobEnd] = growth::sampleBackdatedInterval(
      rng, input.startDate, rules.tenure.job.min, rules.tenure.job.max);

  const double baseSalary =
      input.salarySource() * growth::sampleLognormalMultiplier(rng, 0.03);

  return Employment{
      .employerAcct = employer,
      .payroll = payroll,
      .start = jobStart,
      .end = jobEnd,
      .annualSalary = baseSalary,
      .switchIndex = 0,
  };
}

/// Transition to a new job with tenure, cadence, and salary bump.
[[nodiscard]] inline Employment
advanceEmployment(const EmploymentRules &rules,
                  const random::RngFactory &factory, random::Rng &rng,
                  const EmploymentAdvanceInput &input) {
  const auto employer =
      growth::pickDifferent(rng, input.employers, input.previous.employerAcct);
  const auto payroll = samplePayrollProfile(rules.payroll, factory, employer);
  const auto [start, end] = growth::sampleForwardInterval(
      rng, input.now, rules.tenure.job.min, rules.tenure.job.max);

  const double currentSalary =
      input.previous.annualSalary *
      detail::compoundSalary(rules, factory,
                             detail::SalaryGrowthInput{
                                 .personId = input.personId,
                                 .start = input.previous.start,
                                 .now = input.now,
                             });

  const double bump = detail::jobSwitchBump(
      factory, rules.raises, input.personId, input.previous.switchIndex + 1);

  const double newSalary = currentSalary * (1.0 + bump);

  return Employment{
      .employerAcct = employer,
      .payroll = payroll,
      .start = start,
      .end = end,
      .annualSalary = newSalary,
      .switchIndex = input.previous.switchIndex + 1,
  };
}

/// Calculate the exact paycheck amount for a given pay date.
[[nodiscard]] inline double calculateSalary(const EmploymentRules &rules,
                                            const random::RngFactory &factory,
                                            const SalaryQuery &query) {
  const double annual = query.state.annualSalary *
                        detail::compoundSalary(rules, factory,
                                               detail::SalaryGrowthInput{
                                                   .personId = query.personId,
                                                   .start = query.state.start,
                                                   .now = query.payDate,
                                               });

  const auto cal = time::toCalendarDate(query.payDate);
  const int periods = payPeriodsInYear(query.state.payroll, cal.year);

  return math::roundMoney(
      std::max(50.0, annual / static_cast<double>(periods)));
}

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
