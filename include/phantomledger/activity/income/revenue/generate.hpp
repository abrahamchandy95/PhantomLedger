#pragma once

#include "phantomledger/activity/income/revenue/flows.hpp"
#include "phantomledger/activity/income/revenue/sources.hpp"
#include "phantomledger/activity/income/types.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstddef>
#include <string>
#include <string_view>
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
  Generator(const Book &book, const transactions::Factory &txf)
      : book_(book), txf_(txf),
        window_{book.timeframe.startDate, book.timeframe.days} {
    txns_.reserve(static_cast<std::size_t>(book.population.count()) *
                  book.timeframe.monthStarts.size() * 3);
  }

  [[nodiscard]] std::vector<transactions::Transaction> run() {
    if (!book_.counterparties.available()) {
      return {};
    }

    for (PersonId person = 1; person <= book_.population.count(); ++person) {
      runPerson(person);
    }

    sortTransfers(txns_);
    return std::move(txns_);
  }

private:
  void runPerson(PersonId person) {
    const auto plan = source::assign(book_, person);
    if (!plan.has_value() || !plan->sources.any() || !plan->valid()) {
      return;
    }

    const auto personKey = std::to_string(static_cast<unsigned>(person));

    for (const auto &monthStart : book_.timeframe.monthStarts) {
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

    flow::Cycle cycle{rng, window_, monthStart, txf_};

    cycle.clients(profile.client, plan.accounts.revenueDst,
                  plan.sources.clients);
    cycle.platforms(profile.platform, plan.accounts.revenueDst,
                    plan.sources.platforms);
    cycle.settlements(profile.settlement, plan.accounts.revenueDst,
                      plan.sources.processor);
    cycle.draws(profile.ownerDraw, plan.accounts.personal,
                plan.sources.drawSrc);
    cycle.investments(profile.investment, plan.accounts.personal,
                      plan.sources.investmentSrc);

    std::move(cycle).drainInto(txns_);
  }

  [[nodiscard]] random::Rng monthRng(std::string_view personKey,
                                     time::TimePoint monthStart) const {
    const auto cal = time::toCalendarDate(monthStart);
    const auto yearStr = std::to_string(cal.year);
    const auto monthStr = std::to_string(static_cast<int>(cal.month));

    return book_.entropy.factory.rng(
        {"legit", "nonpayroll_income", personKey, yearStr, monthStr});
  }

  const Book &book_;
  const transactions::Factory &txf_;
  const time::Window window_;
  std::vector<transactions::Transaction> txns_;
};

} // namespace detail

/// Generate all non-payroll revenue transactions for the simulation.
[[nodiscard]] inline std::vector<transactions::Transaction>
generate(const Book &book, const transactions::Factory &txf) {
  return detail::Generator(book, txf).run();
}

} // namespace PhantomLedger::inflows::revenue
