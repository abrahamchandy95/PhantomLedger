#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/spending/actors/event.hpp"
#include "phantomledger/spending/market/market.hpp"
#include "phantomledger/spending/routing/emission_result.hpp"
#include "phantomledger/spending/routing/policy.hpp"

#include <optional>

namespace PhantomLedger::spending::routing {

[[nodiscard]] EmissionResult
emitBill(random::Rng &rng, const market::Market &market, const Policy &policy,
         const ResolvedAccounts &resolved, const actors::Event &event);

[[nodiscard]] EmissionResult emitExternal(random::Rng &rng,
                                          const market::Market &market,
                                          const ResolvedAccounts &resolved,
                                          const actors::Event &event);

[[nodiscard]] std::optional<EmissionResult>
emitMerchant(random::Rng &rng, const market::Market &market,
             const Policy &policy, const ResolvedAccounts &resolved,
             const actors::Event &event);

[[nodiscard]] std::optional<EmissionResult>
emitP2p(random::Rng &rng, const market::Market &market,
        const ResolvedAccounts &resolved, const actors::Event &event);

} // namespace PhantomLedger::spending::routing
