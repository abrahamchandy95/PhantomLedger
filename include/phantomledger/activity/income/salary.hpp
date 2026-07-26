#pragma once

#include "phantomledger/activity/income/selection.hpp"
#include "phantomledger/activity/income/timestamps.hpp"
#include "phantomledger/activity/income/types.hpp"
#include "phantomledger/activity/recurring/employment.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/synth/personas/timeline.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <functional>
#include <string_view>
#include <vector>

namespace PhantomLedger::activity::income {

namespace salary {

namespace enumTax = ::PhantomLedger::taxonomies::enums;
namespace tlx = ::PhantomLedger::synth::personas::timeline;

inline const channels::Tag channel = channels::tag(channels::Legit::salary);

struct Rules {
  recurring::EmploymentRules employment{};
  // paidFraction is the fitScale TARGET for the mean selection
  // probability across ALL candidates, not a per-persona rate. It must
  // sit at the persona table's weighted mean so the fitted scale ~= 1.0
  // and the table below IS the effective per-persona employment
  // probability (household-econ-2026-07 finding; L-4).
  //
  // H2 step 2b re-derivation (declared; authority U-7): selection now
  // keys on the timeline's WORKING-LIFE type, so the weighted mean is
  //   .60x.98 (salaried) + .12x(.85x.98+.15x.08) (student destinations)
  //   + .10x.02 (retiree) + .10x.08 (freelancer)
  //   + .06x(.70x.98+.30x.08) (post-business destinations)
  //   + .02x.12 (HNW)  ~= .744.
  // The old .65 sat at the SEED-persona mean.
  double paidFraction = 0.74;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;

    employment.validate(r);
    r.check([&] { v::unit("salaryPaidFraction", paidFraction); });
  }
};

struct Payroll {
  Timeframe timeframe;
  Entropy entropy;
  Population population;
  PayrollCounterparties counterparties;
  Rules rules{};

  void validate(primitives::validate::Report &r) const {
    timeframe.validate(r);
    rules.validate(r);
  }
};

namespace internal {

struct PersonaProbability {
  personas::Type persona;
  double probability = 0.0;
};

// Effective employment probabilities (see the Rules::paidFraction
// note): student .40 = NCES/BLS full-time-student employment;
// retiree .02 = fully retired persona (Social Security income is the
// separate government benefits stream, RetirementTerms); freelancer/
// smallBusiness stay low because their income arrives through the
// L-10 revenue profiles, not payroll.
//
// H2 step 2b: the row applied to a person is their timeline's
// WORKING-LIFE type (tl.working), not the seed — a student's
// destination (salaried .85 / freelancer .15), a small-business
// owner's post-close type (.70/.30). Paydays then run ONLY inside
// [payrollStart(tl), tl.retirement): the student .40 row therefore
// applies to nobody (study-period jobs are out of scope at H2 —
// family support carries students; the "student wage tier" remains
// the registered owner-side gap), and the retiree .02 row selects
// people whose payroll interval is empty (zero rows) — both declared
// in docs/h2_persona_timeline.md.
inline constexpr auto kPersonaProbabilities =
    std::to_array<PersonaProbability>({
        {personas::Type::student, 0.40},
        {personas::Type::retiree, 0.02},
        {personas::Type::freelancer, 0.08},
        {personas::Type::smallBusiness, 0.04},
        {personas::Type::highNetWorth, 0.12},
        {personas::Type::salaried, 0.98},
    });

struct ProbabilityBuild {
  std::array<double, personas::kKindCount> values{};
  bool valid = true;
};

[[nodiscard]] consteval bool unit(double value) {
  return value >= 0.0 && value <= 1.0;
}

[[nodiscard]] consteval ProbabilityBuild buildProbabilityTable() {
  ProbabilityBuild build{};
  std::array<bool, personas::kKindCount> seen{};

  if (kPersonaProbabilities.size() != personas::kKindCount) {
    build.valid = false;
    return build;
  }

  for (const auto entry : kPersonaProbabilities) {
    const auto index = enumTax::toIndex(entry.persona);

    if (index >= personas::kKindCount) {
      build.valid = false;
      return build;
    }

    if (seen[index]) {
      build.valid = false;
      return build;
    }

    if (!unit(entry.probability)) {
      build.valid = false;
      return build;
    }

    build.values[index] = entry.probability;
    seen[index] = true;
  }

  for (const bool found : seen) {
    if (!found) {
      build.valid = false;
      return build;
    }
  }

  return build;
}

inline constexpr auto kProbabilityBuild = buildProbabilityTable();

static_assert(kProbabilityBuild.valid,
              "salary probabilities must contain exactly one valid entry for "
              "each persona");

inline constexpr auto kProbabilityByPersona = kProbabilityBuild.values;

[[nodiscard]] constexpr double probabilityFor(personas::Type type) noexcept {
  return kProbabilityByPersona[enumTax::toIndex(type)];
}

} // namespace internal

struct NumText {
  std::array<char, 16> buf{};
  std::size_t len{};

  explicit NumText(unsigned value) noexcept {
    auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
    (void)ec;
    len = static_cast<std::size_t>(ptr - buf.data());
  }

  [[nodiscard]] std::string_view str() const noexcept {
    return {buf.data(), len};
  }
};

[[nodiscard]] inline bool candidate(const Population &population,
                                    PersonId person) {
  return population.exists(person) && population.hasAccount(person) &&
         !population.isHub(person);
}

[[nodiscard]] inline double baseProbability(const Population &population,
                                            PersonId person) {
  // H2 step 2b: the probability of the WORKING-LIFE type (timeline).
  return internal::probabilityFor(population.timeline(person).working);
}

class Paymaster {
public:
  Paymaster(const Payroll &payroll, random::Rng &rng,
            const transactions::Factory &txf,
            const std::function<double()> &salaryModel)
      : payroll_(payroll), rng_(rng), txf_(txf), salaryModel_(salaryModel),
        salaryGrowth_(payroll.rules.employment.salary, payroll.entropy.factory),
        init_(payroll.rules.employment.job, payroll.rules.employment.payroll,
              payroll.entropy.factory),
        advance_(payroll.rules.employment.job, payroll.rules.employment.payroll,
                 salaryGrowth_, payroll.entropy.factory),
        salaryCalc_(salaryGrowth_) {}

  void pay(PersonId person, std::vector<transactions::Transaction> &out) {
    // H2 step 2b: paydays exist only inside the person's payroll-active
    // life [payrollStart(tl), tl.retirement) intersected with the
    // window. Retired-for-the-whole-window people (seed retirees)
    // return BEFORE the salary-level draw, students anchor their first
    // job at the study->work transition, small-business owners at the
    // business close. H3: the interval additionally ends at DEATH —
    // no paycheck posts after tl.death (the last employer pay period
    // is simply cut; final-pay settlement is a declared
    // simplification away).
    const auto &tl = payroll_.population.timeline(person);
    const auto activeStart =
        std::max(payroll_.timeframe.startDate, tlx::payrollStart(tl));
    const auto activeEnd = std::min(
        {payroll_.timeframe.end(), tl.retirement, tl.death});
    if (activeEnd <= activeStart) {
      return;
    }

    const auto dst = payroll_.population.primary(person);
    const NumText personId(static_cast<unsigned>(person));
    const auto personIdText = personId.str();

    auto state = start(personIdText, activeStart);

    while (true) {
      payJob(state, dst, personIdText, activeStart, activeEnd, out);

      if (state.end >= activeEnd) {
        break;
      }

      state = next(state, personIdText);
    }
  }

private:
  // `anchor` = the payroll-active start: the backdated first-job
  // interval samples around it (for a mid-window career start the
  // "job" nominally began earlier; paydates are clipped to the active
  // interval, and the slightly longer tenure only feeds the
  // idiosyncratic raise compounding — declared).
  [[nodiscard]] recurring::Employment start(std::string_view personId,
                                            time::TimePoint anchor) const {
    recurring::SalarySource annualSalary = [this]() -> double {
      return salaryModel_() * 12.0;
    };

    return init_(personId, anchor, payroll_.counterparties.employers,
                 annualSalary);
  }

  void payJob(const recurring::Employment &state, const Key &dst,
              std::string_view personId, time::TimePoint activeStart,
              time::TimePoint activeEnd,
              std::vector<transactions::Transaction> &out) {
    const auto &timeframe = payroll_.timeframe;
    const auto segmentEnd = std::min(state.end, activeEnd);

    for (const auto &payDate :
         recurring::paydatesForWindow(state, activeStart, segmentEnd)) {
      const auto ts =
          timestamps::jittered(payDate, state.payroll.postingLagDays,
                               timestamps::kSalaryTimestampJitter, rng_);

      if (!timeframe.contains(ts)) {
        continue;
      }

      const double amount = salaryCalc_(personId, state, payDate);

      out.push_back(txf_.make(transactions::Draft{
          .source = state.employerAcct,
          .destination = dst,
          .amount = amount,
          .timestamp = time::toEpochSeconds(ts),
          .isFraud = 0,
          .ringId = -1,
          .channel = salary::channel,
      }));
    }
  }

  [[nodiscard]] recurring::Employment next(const recurring::Employment &state,
                                           std::string_view personId) const {
    const NumText switchId(static_cast<unsigned>(state.switchIndex));

    auto advRng = payroll_.entropy.factory.rng(
        {"employment_advance", personId, switchId.str()});

    return advance_(advRng, personId, state.end,
                    payroll_.counterparties.employers, state);
  }

  const Payroll &payroll_;
  random::Rng &rng_;
  const transactions::Factory &txf_;
  const std::function<double()> &salaryModel_;

  recurring::SalaryGrowthModel salaryGrowth_;
  recurring::EmploymentInitializer init_;
  recurring::EmploymentAdvancer advance_;
  recurring::SalaryCalculator salaryCalc_;
};

} // namespace salary

[[nodiscard]] inline std::vector<transactions::Transaction>
generateSalaryTxns(const salary::Payroll &payroll, random::Rng &rng,
                   const transactions::Factory &txf,
                   const std::function<double()> &salaryModel) {
  if (payroll.counterparties.employers.empty()) {
    return {};
  }

  primitives::validate::require(payroll);

  const auto selector = selection::makeSelector(
      [&](PersonId person) {
        return salary::candidate(payroll.population, person);
      },
      [&](PersonId person) {
        return salary::baseProbability(payroll.population, person);
      });

  const double scale =
      selector.fitScale(payroll.population.count(), payroll.rules.paidFraction);

  if (scale <= 0.0) {
    return {};
  }

  std::vector<transactions::Transaction> txns;
  txns.reserve(payroll.population.count() * 2);

  salary::Paymaster paymaster(payroll, rng, txf, salaryModel);

  for (PersonId person = 1; person <= payroll.population.count(); ++person) {
    if (!selector.selected(rng, person, scale)) {
      continue;
    }

    paymaster.pay(person, txns);
  }

  sortTransfers(txns);

  return txns;
}

} // namespace PhantomLedger::activity::income
