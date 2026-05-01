#include "phantomledger/transfers/legit/ledger/burdens.hpp"

#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/taxonomies/products/predicates.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace PhantomLedger::transfers::legit::ledger {

namespace {

namespace products = ::PhantomLedger::products;

[[nodiscard]] bool contributesToBurden(products::ProductType type) noexcept {
  return products::isInstallmentLoan(type) ||
         type == products::ProductType::tax;
}

[[nodiscard]] time::TimePoint addMonths(time::TimePoint timePoint,
                                        int months) noexcept {
  return time::addDays(timePoint, months * 30);
}

} // namespace

std::vector<double>
buildMonthlyBurdens(const entity::product::PortfolioRegistry &registry,
                    std::uint32_t personCount, time::TimePoint windowStart) {
  std::vector<double> burdens(personCount, 0.0);
  if (personCount == 0) {
    return burdens;
  }

  // ---- Pass 1: obligations stream (loans + tax) ----
  const auto windowEndExcl = addMonths(windowStart, kBurdenWindowMonths);
  const auto events = registry.allEvents(windowStart, windowEndExcl);

  for (const auto &event : events) {
    if (event.direction != entity::product::Direction::outflow) {
      continue;
    }

    if (!contributesToBurden(event.productType)) {
      continue;
    }

    const auto personIdx = static_cast<std::size_t>(event.personId) - 1;
    if (personIdx >= burdens.size()) {
      continue;
    }

    burdens[personIdx] += event.amount;
  }

  // Three-month window normalised back to a per-month figure.
  for (auto &burden : burdens) {
    burden /= static_cast<double>(kBurdenWindowMonths);
  }

  // ---- Pass 2: insurance premiums (skip home if person has mortgage) ----
  registry.forEachInsuredPerson(
      [&](entity::PersonId person,
          const entity::product::InsuranceHoldings &holdings) {
        const auto idx = static_cast<std::size_t>(person) - 1;
        if (idx >= burdens.size()) {
          return;
        }

        if (const auto &policy = holdings.autoPolicy(); policy.has_value()) {
          burdens[idx] += policy->monthlyPremium;
        }

        if (const auto &policy = holdings.lifePolicy(); policy.has_value()) {
          burdens[idx] += policy->monthlyPremium;
        }

        if (const auto &policy = holdings.homePolicy();
            policy.has_value() && !registry.hasMortgage(person)) {
          burdens[idx] += policy->monthlyPremium;
        }
      });

  return burdens;
}

} // namespace PhantomLedger::transfers::legit::ledger
