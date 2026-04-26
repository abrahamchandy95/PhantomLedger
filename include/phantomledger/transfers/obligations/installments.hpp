#pragma once
/*
  The state machine itself lives in `delinquency.hpp` and is pure
  data → data. This header is the *bridge* between that machine and
  the transaction stream.
 */

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/products/event.hpp"
#include "phantomledger/entities/products/portfolio.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/obligations/delinquency.hpp"
#include "phantomledger/transfers/obligations/jitter.hpp"

#include <vector>

namespace PhantomLedger::transfers::obligations::installments {

inline void postEvent(std::vector<transactions::Transaction> &out,
                      const entity::product::PortfolioRegistry &registry,
                      delinquency::StateMap &stateMap, random::Rng &rng,
                      const transactions::Factory &factory,
                      const entity::product::ObligationEvent &event,
                      const entity::Key &personAcct, time::TimePoint endExcl) {
  const auto *terms =
      registry.installmentTerms(event.personId, event.productType);
  if (terms == nullptr) {
    return;
  }

  const double scheduled = primitives::utils::roundMoney(event.amount);
  const delinquency::StateKey key{event.personId, event.productType};
  auto &state = stateMap[key];

  const auto outcome = delinquency::advance(rng, state, *terms, scheduled);
  if (outcome.action == delinquency::Action::noPayment) {
    return;
  }

  const auto ts = jitter::installmentTimestamp(
      rng, event.timestamp, outcome.effectiveLateP, terms->lateDaysMin,
      terms->lateDaysMax, outcome.forceLate);
  if (ts >= endExcl) {
    return;
  }

  out.push_back(factory.make(transactions::Draft{
      .source = personAcct,
      .destination = event.counterpartyAcct,
      .amount = primitives::utils::roundMoney(outcome.amount),
      .timestamp = time::toEpochSeconds(ts),
      .channel = event.channel,
  }));
}
} // namespace PhantomLedger::transfers::obligations::installments
