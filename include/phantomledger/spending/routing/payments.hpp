#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/spending/actors/event.hpp"
#include "phantomledger/spending/market/market.hpp"
#include "phantomledger/spending/routing/policy.hpp"
#include "phantomledger/transactions/record.hpp"

#include <optional>

namespace PhantomLedger::spending::routing {

[[nodiscard]] transactions::Transaction emitBill(random::Rng &rng,
                                                 const market::Market &market,
                                                 const Policy &policy,
                                                 const actors::Event &event);

[[nodiscard]] transactions::Transaction
emitExternal(random::Rng &rng, const market::Market &market,
             const actors::Event &event);

[[nodiscard]] std::optional<transactions::Transaction>
emitMerchant(random::Rng &rng, const market::Market &market,
             const Policy &policy, const actors::Event &event);

[[nodiscard]] std::optional<transactions::Transaction>
emitP2p(random::Rng &rng, const market::Market &market,
        const actors::Event &event);

} // namespace PhantomLedger::spending::routing
