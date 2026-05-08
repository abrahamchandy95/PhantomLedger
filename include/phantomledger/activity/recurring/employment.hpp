#pragma once

#include "phantomledger/activity/recurring/growth.hpp"
#include "phantomledger/activity/recurring/payroll.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/distributions/cdf.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/taxonomies/recurring/types.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <span>
#include <stdexcept>
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

struct JobRules {
  growth::Range tenure{
      .min = 1.5,
      .max = 4.0,
  };

  void validate(primitives::validate::Report &r) const { tenure.validate(r); }
};

struct SalaryGrowthRules {
  growth::CompoundRules annual{
      .annualInflation = 0.025,
      .annualRaise =
          growth::GrowthDist{
              .mu = 0.015,
              .sigma = 0.020,
              .floor = -0.02,
          },
  };

  growth::GrowthDist switchBump{
      .mu = 0.08,
      .sigma = 0.06,
      .floor = -0.05,
  };

  void validate(primitives::validate::Report &r) const {
    annual.validate(r);
    switchBump.validate(r);
  }
};

struct EmploymentRules {
  JobRules job{};
  SalaryGrowthRules salary{};
  PayrollRules payroll{};

  void validate(primitives::validate::Report &r) const {
    job.validate(r);
    salary.validate(r);
    payroll.validate(r);
  }
};

namespace detail {

inline void requireSalarySource(const SalarySource &source,
                                std::string_view field) {
  if (!source) {
    throw std::invalid_argument(std::string(field) + " is required");
  }
}

inline void requireSalaryAmount(double amount, std::string_view field) {
  namespace v = primitives::validate;

  v::finite(field, amount);
  v::positive(field, amount);
}

[[nodiscard]] inline double
sampleInitialAnnualSalary(random::Rng &rng, const SalarySource &source) {
  requireSalarySource(source, "salarySource");

  const double base = source();
  requireSalaryAmount(base, "salarySource");

  const double multiplier = growth::sampleLognormalMultiplier(rng, 0.03);
  const double salary = base * multiplier;
  requireSalaryAmount(salary, "annualSalary");

  return salary;
}

} // namespace detail

/// Sample a payroll profile for an employer account.
[[nodiscard]] inline PayrollProfile
samplePayrollProfile(const PayrollRules &rules,
                     const random::RngFactory &factory,
                     const entity::Key &employerAcct) {
  primitives::validate::require(rules);

  const auto key = std::to_string(employerAcct.number);
  auto rng = factory.rng({"employer_payroll_profile", key});

  const auto &weights = rules.weights;
  const std::array<double, kPayCadenceCount> cadenceWeights = {
      weights.weekly,
      weights.biweekly,
      weights.semimonthly,
      weights.monthly,
  };

  const auto cdf = distributions::buildCdf(cadenceWeights);
  const auto cadence =
      kPayCadences[distributions::sampleIndex(cdf, rng.nextDouble())];

  int weekday = rules.defaultWeekday;
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

  const int lagMax = rules.postingLagDaysMax;
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

  primitives::validate::require(profile);
  return profile;
}

class SalaryGrowthModel {
public:
  SalaryGrowthModel(const SalaryGrowthRules &rules,
                    const random::RngFactory &factory)
      : rules_(rules), factory_(factory) {
    primitives::validate::require(rules_);
  }

  [[nodiscard]] double compound(std::string_view personId,
                                time::TimePoint start,
                                time::TimePoint now) const {
    const growth::AnnualRaiseSeries raises{factory_, "salary_real_raise",
                                           personId};
    return growth::compoundGrowth(rules_.annual, raises, start, now);
  }

  [[nodiscard]] double jobSwitchBump(std::string_view personId,
                                     int switchIndex) const {
    auto rng = factory_.rng(
        {"job_switch_bump", personId, std::to_string(switchIndex)});

    return growth::sampleNormalClamped(rng, rules_.switchBump);
  }

private:
  const SalaryGrowthRules &rules_;
  const random::RngFactory &factory_;
};

class EmploymentInitializer {
public:
  EmploymentInitializer(const JobRules &jobRules,
                        const PayrollRules &payrollRules,
                        const random::RngFactory &factory)
      : jobRules_(jobRules), payrollRules_(payrollRules), factory_(factory) {
    primitives::validate::require(jobRules_);
    primitives::validate::require(payrollRules_);
  }

  [[nodiscard]] Employment operator()(std::string_view personId,
                                      time::TimePoint startDate,
                                      std::span<const entity::Key> employers,
                                      const SalarySource &salarySource) const {
    auto rng = factory_.rng({"employment_init", personId});

    const auto employer = growth::pickOne(rng, employers);
    const auto payroll =
        samplePayrollProfile(payrollRules_, factory_, employer);

    const auto interval =
        growth::sampleBackdatedInterval(rng, startDate, jobRules_.tenure);

    const double annualSalary =
        detail::sampleInitialAnnualSalary(rng, salarySource);

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
  const JobRules &jobRules_;
  const PayrollRules &payrollRules_;
  const random::RngFactory &factory_;
};

class EmploymentAdvancer {
public:
  EmploymentAdvancer(const JobRules &jobRules, const PayrollRules &payrollRules,
                     const SalaryGrowthModel &salaryGrowth,
                     const random::RngFactory &factory)
      : jobRules_(jobRules), payrollRules_(payrollRules),
        salaryGrowth_(salaryGrowth), factory_(factory) {
    primitives::validate::require(jobRules_);
    primitives::validate::require(payrollRules_);
  }

  [[nodiscard]] Employment operator()(random::Rng &rng,
                                      std::string_view personId,
                                      time::TimePoint now,
                                      std::span<const entity::Key> employers,
                                      const Employment &previous) const {
    detail::requireSalaryAmount(previous.annualSalary, "previous.annualSalary");

    const auto employer =
        growth::pickDifferent(rng, employers, previous.employerAcct);

    const auto payroll =
        samplePayrollProfile(payrollRules_, factory_, employer);

    const auto interval =
        growth::sampleForwardInterval(rng, now, jobRules_.tenure);

    const double currentSalary =
        previous.annualSalary *
        salaryGrowth_.compound(personId, previous.start, now);

    detail::requireSalaryAmount(currentSalary, "currentSalary");

    const double bump =
        salaryGrowth_.jobSwitchBump(personId, previous.switchIndex + 1);

    const double factor = 1.0 + bump;
    growth::requireGrowthFactor(factor, "jobSwitchFactor");

    const double nextSalary = currentSalary * factor;
    detail::requireSalaryAmount(nextSalary, "nextAnnualSalary");

    return Employment{
        .employerAcct = employer,
        .payroll = payroll,
        .start = interval.start,
        .end = interval.end,
        .annualSalary = nextSalary,
        .switchIndex = previous.switchIndex + 1,
    };
  }

private:
  const JobRules &jobRules_;
  const PayrollRules &payrollRules_;
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
    detail::requireSalaryAmount(state.annualSalary, "state.annualSalary");
    primitives::validate::require(state.payroll);

    const double annual =
        state.annualSalary *
        salaryGrowth_.compound(personId, state.start, payDate);

    detail::requireSalaryAmount(annual, "annualSalary");

    const auto cal = time::toCalendarDate(payDate);
    const int periods = payPeriodsInYear(state.payroll, cal.year);

    return primitives::utils::roundMoney(
        std::max(50.0, annual / static_cast<double>(periods)));
  }

private:
  const SalaryGrowthModel &salaryGrowth_;
};

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
