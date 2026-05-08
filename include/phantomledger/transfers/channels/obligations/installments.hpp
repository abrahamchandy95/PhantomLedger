#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/products/event.hpp"
#include "phantomledger/entities/products/portfolio.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/taxonomies/products/predicates.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/channels/obligations/delinquency.hpp"
#include "phantomledger/transfers/channels/obligations/jitter.hpp"

#include <optional>

namespace PhantomLedger::transfers::obligations::installments {

[[nodiscard]] inline bool
tracks(const entity::product::PortfolioRegistry &registry,
       const entity::product::ObligationEvent &event) noexcept {
  return event.direction == entity::product::Direction::outflow &&
         ::PhantomLedger::products::isInstallmentLoan(event.productType) &&
         registry.installmentTerms(event.personId, event.productType) !=
             nullptr;
}

class EventEmitter {
public:
  explicit EventEmitter(
      const entity::product::PortfolioRegistry &registry) noexcept
      : registry_(&registry) {}

  [[nodiscard]] std::optional<transactions::Draft>
  draftFor(random::Rng &rng, const entity::product::ObligationEvent &event,
           const entity::Key &personAcct, time::TimePoint endExcl) {
    const auto *terms =
        registry_->installmentTerms(event.personId, event.productType);
    if (terms == nullptr) {
      return std::nullopt;
    }

    const double scheduled = primitives::utils::roundMoney(event.amount);
    const delinquency::StateKey key{event.personId, event.productType};
    auto &state = stateMap_[key];

    const auto outcome = delinquency::advance(rng, state, *terms, scheduled);
    if (outcome.action == delinquency::Action::noPayment) {
      return std::nullopt;
    }

    const auto ts = jitter::installmentTimestamp(
        rng, event.timestamp, outcome.effectiveLateP, terms->lateDaysMin,
        terms->lateDaysMax, outcome.forceLate);
    if (ts >= endExcl) {
      return std::nullopt;
    }

    return transactions::Draft{
        .source = personAcct,
        .destination = event.counterpartyAcct,
        .amount = primitives::utils::roundMoney(outcome.amount),
        .timestamp = time::toEpochSeconds(ts),
        .channel = event.channel,
    };
  }

private:
  const entity::product::PortfolioRegistry *registry_ = nullptr;
  delinquency::StateMap stateMap_{};
};

} // namespace PhantomLedger::transfers::obligations::installments
