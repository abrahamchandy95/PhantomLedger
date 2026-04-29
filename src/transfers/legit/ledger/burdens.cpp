#include "phantomledger/transfers/legit/ledger/burdens.hpp"

#include "phantomledger/primitives/time/calendar.hpp"

#include <algorithm>

namespace PhantomLedger::transfers::legit::ledger {

namespace {

[[nodiscard]] bool
contributesToBurden(entity::product::ProductType type) noexcept {
  using entity::product::ProductType;
  switch (type) {
  case ProductType::mortgage:
  case ProductType::autoLoan:
  case ProductType::studentLoan:
  case ProductType::tax:
    return true;
  case ProductType::insurance: // walked separately
  case ProductType::unknown:
    return false;
  }
  return false;
}

[[nodiscard]] time::TimePoint addMonths(time::TimePoint tp,
                                        int months) noexcept {

  return time::addDays(tp, months * 30);
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
  for (auto &b : burdens) {
    b /= static_cast<double>(kBurdenWindowMonths);
  }

  // ---- Pass 2: insurance premiums (skip home if person has mortgage) ----
  registry.forEachInsuredPerson(
      [&](entity::PersonId person,
          const entity::product::InsuranceHoldings &h) {
        const auto idx = static_cast<std::size_t>(person) - 1;
        if (idx >= burdens.size()) {
          return;
        }

        if (h.auto_.has_value()) {
          burdens[idx] += h.auto_->monthlyPremium;
        }
        if (h.life.has_value()) {
          burdens[idx] += h.life->monthlyPremium;
        }
        if (h.home.has_value() && !registry.hasMortgage(person)) {
          burdens[idx] += h.home->monthlyPremium;
        }
      });

  return burdens;
}

} // namespace PhantomLedger::transfers::legit::ledger
