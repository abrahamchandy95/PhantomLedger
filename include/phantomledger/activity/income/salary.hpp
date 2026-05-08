#pragma once

#include "phantomledger/activity/income/selection.hpp"
#include "phantomledger/activity/income/timestamps.hpp"
#include "phantomledger/activity/income/types.hpp"
#include "phantomledger/activity/recurring/employment.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
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

namespace PhantomLedger::inflows {

namespace salary {

namespace enumTax = ::PhantomLedger::taxonomies::enums;

inline const channels::Tag channel = channels::tag(channels::Legit::salary);

// ---------------------------------------------------------------
// Salary subsystem rules. Owned here, not in a cross-cutting bag.
// ---------------------------------------------------------------

struct Rules {
  recurring::EmploymentRules employment{};
  double paidFraction = 0.95;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;

    employment.validate(r);
    r.check([&] { v::unit("salaryPaidFraction", paidFraction); });
  }
};

// ---------------------------------------------------------------
// Payroll: the narrow view salary generation needs.
// Replaces the old `InflowSnapshot` for this subsystem.
// ---------------------------------------------------------------

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

inline constexpr auto kPersonaProbabilities =
    std::to_array<PersonaProbability>({
        {personas::Type::student, 0.12},
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
  return internal::probabilityFor(population.persona(person));
}

class Paymaster {
public:
  Paymaster(const Payroll &payroll, random::Rng &rng,
            const transactions::Factory &txf,
            const std::function<double()> &salaryModel,
            std::vector<transactions::Transaction> &out)
      : payroll_(payroll), rng_(rng), txf_(txf), salaryModel_(salaryModel),
        out_(out),
        salaryGrowth_(payroll.rules.employment.salary, payroll.entropy.factory),
        init_(payroll.rules.employment.job, payroll.rules.employment.payroll,
              payroll.entropy.factory),
        advance_(payroll.rules.employment.job, payroll.rules.employment.payroll,
                 salaryGrowth_, payroll.entropy.factory),
        salaryCalc_(salaryGrowth_) {}

  void pay(PersonId person) {
    const auto dst = payroll_.population.primary(person);
    const NumText personId(static_cast<unsigned>(person));
    const auto personIdText = personId.str();

    auto state = start(personIdText);
    const auto windowEnd = payroll_.timeframe.end();

    while (true) {
      payJob(state, dst, personIdText);

      if (state.end >= windowEnd) {
        break;
      }

      state = next(state, personIdText);
    }
  }

private:
  [[nodiscard]] recurring::Employment start(std::string_view personId) const {
    recurring::SalarySource annualSalary = [this]() -> double {
      return salaryModel_() * 12.0;
    };

    return init_(personId, payroll_.timeframe.startDate,
                 payroll_.counterparties.employers, annualSalary);
  }

  void payJob(const recurring::Employment &state, const Key &dst,
              std::string_view personId) {
    const auto &timeframe = payroll_.timeframe;
    const auto segmentEnd = std::min(state.end, timeframe.end());

    for (const auto &payDate :
         recurring::paydatesForWindow(state, timeframe.startDate, segmentEnd)) {
      payDateTx(state, dst, personId, payDate);
    }
  }

  void payDateTx(const recurring::Employment &state, const Key &dst,
                 std::string_view personId, time::TimePoint payDate) {
    const auto ts =
        timestamps::jittered(payDate, state.payroll.postingLagDays,
                             timestamps::kSalaryTimestampJitter, rng_);

    if (!payroll_.timeframe.contains(ts)) {
      return;
    }

    const double amount = salaryCalc_(personId, state, payDate);

    out_.push_back(txf_.make(transactions::Draft{
        .source = state.employerAcct,
        .destination = dst,
        .amount = amount,
        .timestamp = time::toEpochSeconds(ts),
        .isFraud = 0,
        .ringId = -1,
        .channel = salary::channel,
    }));
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
  std::vector<transactions::Transaction> &out_;

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
      selector.fitScale(payroll.population.count, payroll.rules.paidFraction);

  if (scale <= 0.0) {
    return {};
  }

  std::vector<transactions::Transaction> txns;
  txns.reserve(payroll.population.count * 2);

  salary::Paymaster paymaster(payroll, rng, txf, salaryModel, txns);

  for (PersonId person = 1; person <= payroll.population.count; ++person) {
    if (!selector.selected(rng, person, scale)) {
      continue;
    }

    paymaster.pay(person);
  }

  sortTransfers(txns);

  return txns;
}

} // namespace PhantomLedger::inflows
