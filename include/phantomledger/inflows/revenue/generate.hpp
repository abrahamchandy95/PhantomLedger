#pragma once
/*
 * For each eligible person (freelancer, smallBusiness, highNetWorth),
 * iterates month-by-month over the simulation window. Each month:
 *   1. Derives a deterministic per-person-per-month RNG.
 *   2. Checks the quiet-month policy.
 *   3. Runs the five revenue flow drafters.
 */

#include "phantomledger/inflows/revenue/flows.hpp"
#include "phantomledger/inflows/revenue/sources.hpp"
#include "phantomledger/inflows/types.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <string>
#include <utility>
#include <vector>

namespace PhantomLedger::inflows::revenue {

namespace detail {

[[nodiscard]] inline bool quietMonth(random::Rng &rng,
                                     const QuietMonthProfile &profile) {
  return rng.nextDouble() < profile.probability &&
         rng.nextDouble() < profile.skipProbability;
}

class Generator {
public:
  Generator(const InflowSnapshot &snapshot, const transactions::Factory &txf)
      : snapshot_(snapshot), txf_(txf), endExcl_(snapshot.timeframe.end()) {
    txns_.reserve(static_cast<std::size_t>(snapshot.population.count) *
                  snapshot.timeframe.monthStarts.size() * 3);
  }

  [[nodiscard]] std::vector<transactions::Transaction> run() {
    if (!snapshot_.counterparties.hasPools()) {
      return {};
    }

    for (PersonId person = 1; person <= snapshot_.population.count; ++person) {
      runPerson(person);
    }

    sortTransfers(txns_);
    return std::move(txns_);
  }

private:
  void runPerson(PersonId person) {
    const auto plan = source::assign(snapshot_, person);
    if (!plan.has_value() || !plan->sources.any() || !plan->valid()) {
      return;
    }

    const auto personKey = std::to_string(static_cast<unsigned>(person));

    for (const auto &monthStart : snapshot_.timeframe.monthStarts) {
      runMonth(*plan, personKey, monthStart);
    }
  }

  void runMonth(const source::Plan &plan, std::string_view personKey,
                time::TimePoint monthStart) {
    auto rng = monthRng(personKey, monthStart);

    const auto &profile = *plan.profile;
    if (quietMonth(rng, profile.quietMonth)) {
      return;
    }

    flow::Cycle cycle{
        rng, monthStart, snapshot_.timeframe.startDate, endExcl_, txf_, txns_,
    };

    flow::clients(cycle, profile.client, plan.accounts.revenueDst,
                  plan.sources.clients);
    flow::platforms(cycle, profile.platform, plan.accounts.revenueDst,
                    plan.sources.platforms);
    flow::settlements(cycle, profile.settlement, plan.accounts.revenueDst,
                      plan.sources.processor);
    flow::draws(cycle, profile.ownerDraw, plan.accounts.personal,
                plan.sources.drawSrc);
    flow::investments(cycle, profile.investment, plan.accounts.personal,
                      plan.sources.investmentSrc);
  }

  [[nodiscard]] random::Rng monthRng(std::string_view personKey,
                                     time::TimePoint monthStart) const {
    const auto cal = time::toCalendarDate(monthStart);
    const auto yearStr = std::to_string(cal.year);
    const auto monthStr = std::to_string(static_cast<int>(cal.month));

    return snapshot_.entropy.factory.rng(
        {"legit", "nonpayroll_income", personKey, yearStr, monthStr});
  }

  const InflowSnapshot &snapshot_;
  const transactions::Factory &txf_;
  const time::TimePoint endExcl_;
  std::vector<transactions::Transaction> txns_;
};

} // namespace detail

/// Generate all non-payroll revenue transactions for the simulation.
[[nodiscard]] inline std::vector<transactions::Transaction>
generate(const InflowSnapshot &snapshot, const transactions::Factory &txf) {
  return detail::Generator(snapshot, txf).run();
}

} // namespace PhantomLedger::inflows::revenue
