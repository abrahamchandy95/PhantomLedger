#pragma once

#include "phantomledger/inflows/selection.hpp"
#include "phantomledger/inflows/timestamps.hpp"
#include "phantomledger/inflows/types.hpp"
#include "phantomledger/recurring/employment.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <functional>
#include <string_view>
#include <vector>

namespace PhantomLedger::inflows {

namespace salary {

inline constexpr channels::Tag channel = channels::tag(channels::Legit::salary);

struct ProbabilityTable {
  static constexpr std::array<double, personas::kKindCount> table{{
      0.12, // student
      0.02, // retiree
      0.08, // freelancer
      0.04, // smallBusiness
      0.12, // highNetWorth
      0.98, // salaried
  }};

  [[nodiscard]] static constexpr double forKind(personas::Type type) noexcept {
    return table[personas::indexOf(type)];
  }
};

struct NumText {
  std::array<char, 16> buf{};
  std::size_t len{};

  explicit NumText(unsigned value) noexcept {
    auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
    (void)ec;
    len = static_cast<std::size_t>(ptr - buf.data());
  }

  std::string_view str() const noexcept { return {buf.data(), len}; }
};

[[nodiscard]] inline bool candidate(const Population &population,
                                    PersonId person) {
  return population.exists(person) && population.hasAccount(person) &&
         !population.isHub(person);
}

[[nodiscard]] inline double baseProbability(const Population &population,
                                            PersonId person) {
  return ProbabilityTable::forKind(population.persona(person));
}

class Paymaster {
public:
  Paymaster(const InflowSnapshot &snapshot, random::Rng &rng,
            const transactions::Factory &txf,
            const std::function<double()> &salaryModel,
            std::vector<transactions::Transaction> &out)
      : snapshot_(snapshot), rng_(rng), txf_(txf), salaryModel_(salaryModel),
        out_(out),
        salaryGrowth_(snapshot.policy().inflation, snapshot.policy().raises,
                      snapshot.entropy.factory),
        init_(snapshot.policy().tenure, snapshot.policy().payroll,
              snapshot.entropy.factory),
        advance_(snapshot.policy().tenure, snapshot.policy().payroll,
                 salaryGrowth_, snapshot.entropy.factory),
        salaryCalc_(salaryGrowth_) {}

  void pay(PersonId person) {
    const auto dst = snapshot_.population.primary(person);
    const NumText personId(static_cast<unsigned>(person));
    const auto personIdText = personId.str();

    auto state = start(personIdText);
    const auto windowEnd = snapshot_.timeframe.end();

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

    return init_(personId, snapshot_.timeframe.startDate,
                 snapshot_.counterparties.employers, annualSalary);
  }

  void payJob(const recurring::Employment &state, const Key &dst,
              std::string_view personId) {
    const auto &timeframe = snapshot_.timeframe;
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

    if (!snapshot_.timeframe.contains(ts)) {
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

    auto advRng = snapshot_.entropy.factory.rng(
        {"employment_advance", personId, switchId.str()});

    return advance_(advRng, personId, state.end,
                    snapshot_.counterparties.employers, state);
  }

  const InflowSnapshot &snapshot_;
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
generateSalaryTxns(const InflowSnapshot &snapshot, random::Rng &rng,
                   const transactions::Factory &txf, double targetPaidFraction,
                   const std::function<double()> &salaryModel) {
  if (!snapshot.hasRecurringPolicy() ||
      snapshot.counterparties.employers.empty()) {
    return {};
  }

  const auto selector = selection::makeSelector(
      [&](PersonId person) {
        return salary::candidate(snapshot.population, person);
      },
      [&](PersonId person) {
        return salary::baseProbability(snapshot.population, person);
      });

  const double scale =
      selector.fitScale(snapshot.population.count, targetPaidFraction);
  if (scale <= 0.0) {
    return {};
  }

  std::vector<transactions::Transaction> txns;
  txns.reserve(snapshot.population.count * 2);

  salary::Paymaster paymaster(snapshot, rng, txf, salaryModel, txns);

  for (PersonId person = 1; person <= snapshot.population.count; ++person) {
    if (!selector.selected(rng, person, scale)) {
      continue;
    }

    paymaster.pay(person);
  }

  sortTransfers(txns);
  return txns;
}

} // namespace PhantomLedger::inflows
