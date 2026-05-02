#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/spending/actors/event.hpp"
#include "phantomledger/spending/market/market.hpp"
#include "phantomledger/spending/routing/channel.hpp"
#include "phantomledger/spending/routing/emission_result.hpp"
#include "phantomledger/spending/routing/payments.hpp"

#include <optional>

namespace PhantomLedger::spending::routing {

[[nodiscard]] inline std::optional<EmissionResult>
routeTxn(random::Rng &rng, const market::Market &market,
         const PaymentRoutingRules &policy, const ResolvedAccounts &resolved,
         Slot slot, const actors::Event &event) {
  switch (slot) {
  case Slot::merchant:
    return emitMerchant(rng, market, policy, resolved, event);
  case Slot::bill:
    return emitBill(rng, market, policy, resolved, event);
  case Slot::p2p:
    return emitP2p(rng, market, resolved, event);
  case Slot::externalUnknown:
    return emitExternal(rng, market, resolved, event);
  case Slot::kCount:
    break;
  }
  return std::nullopt;
}

} // namespace PhantomLedger::spending::routing
