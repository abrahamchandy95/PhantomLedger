#pragma once
/*
  Plain (non-installment) obligation emitter.
*/

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/products/event.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/obligations/jitter.hpp"

#include <vector>

namespace PhantomLedger::transfers::obligations::plain {

inline void postEvent(std::vector<transactions::Transaction> &out,
                      random::Rng &rng, const transactions::Factory &factory,
                      const entity::product::ObligationEvent &event,
                      const entity::Key &personAcct, time::TimePoint endExcl) {
  const auto ts = jitter::basicJitter(rng, event.timestamp);
  if (ts >= endExcl) {
    return;
  }

  entity::Key src{};
  entity::Key dst{};
  if (event.direction == entity::product::Direction::outflow) {
    src = personAcct;
    dst = event.counterpartyAcct;
  } else {
    src = event.counterpartyAcct;
    dst = personAcct;
  }

  out.push_back(factory.make(transactions::Draft{
      .source = src,
      .destination = dst,
      .amount = primitives::utils::roundMoney(event.amount),
      .timestamp = time::toEpochSeconds(ts),
      .channel = event.channel,
  }));
}
} // namespace PhantomLedger::transfers::obligations::plain
