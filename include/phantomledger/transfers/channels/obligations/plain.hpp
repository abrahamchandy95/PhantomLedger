#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/products/event.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/channels/obligations/jitter.hpp"

#include <optional>

namespace PhantomLedger::transfers::obligations::plain {

[[nodiscard]] inline std::optional<transactions::Draft>
draftFor(random::Rng &rng, const entity::product::ObligationEvent &event,
         const entity::Key &personAcct, time::TimePoint endExcl) {
  const auto ts = jitter::basicJitter(rng, event.timestamp);
  if (ts >= endExcl) {
    return std::nullopt;
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

  return transactions::Draft{
      .source = src,
      .destination = dst,
      .amount = primitives::utils::roundMoney(event.amount),
      .timestamp = time::toEpochSeconds(ts),
      .channel = event.channel,
  };
}

} // namespace PhantomLedger::transfers::obligations::plain
