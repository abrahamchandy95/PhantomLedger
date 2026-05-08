#pragma once

#include "phantomledger/activity/spending/actors/event.hpp"
#include "phantomledger/activity/spending/market/market.hpp"
#include "phantomledger/activity/spending/routing/emission_result.hpp"
#include "phantomledger/entropy/random/rng.hpp"

#include <optional>

namespace PhantomLedger::spending::routing {

struct PaymentRoutingRules {
  double preferKnownBillersP = 0.85;
  std::uint16_t merchantRetryLimit = 6;
};

[[nodiscard]] EmissionResult emitBill(random::Rng &rng,
                                      const market::Market &market,
                                      const PaymentRoutingRules &policy,
                                      const ResolvedAccounts &resolved,
                                      const actors::Event &event);

[[nodiscard]] EmissionResult emitExternal(random::Rng &rng,
                                          const market::Market &market,
                                          const ResolvedAccounts &resolved,
                                          const actors::Event &event);

[[nodiscard]] std::optional<EmissionResult>
emitMerchant(random::Rng &rng, const market::Market &market,
             const PaymentRoutingRules &policy,
             const ResolvedAccounts &resolved, const actors::Event &event);

[[nodiscard]] std::optional<EmissionResult>
emitP2p(random::Rng &rng, const market::Market &market,
        const ResolvedAccounts &resolved, const actors::Event &event);

} // namespace PhantomLedger::spending::routing
