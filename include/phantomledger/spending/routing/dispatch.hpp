#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/spending/actors/event.hpp"
#include "phantomledger/spending/market/market.hpp"
#include "phantomledger/spending/routing/channel.hpp"
#include "phantomledger/spending/routing/payments.hpp"
#include "phantomledger/spending/routing/policy.hpp"
#include "phantomledger/transactions/record.hpp"

#include <optional>

namespace PhantomLedger::spending::routing {

[[nodiscard]] inline std::optional<transactions::Transaction>
routeTxn(random::Rng &rng, const market::Market &market, const Policy &policy,
         Slot slot, const actors::Event &event) {
  switch (slot) {
  case Slot::merchant:
    return emitMerchant(rng, market, policy, event);
  case Slot::bill:
    return emitBill(rng, market, policy, event);
  case Slot::p2p:
    return emitP2p(rng, market, event);
  case Slot::externalUnknown:
    return emitExternal(rng, market, event);
  case Slot::kCount:
    break;
  }
  return std::nullopt;
}

} // namespace PhantomLedger::spending::routing
