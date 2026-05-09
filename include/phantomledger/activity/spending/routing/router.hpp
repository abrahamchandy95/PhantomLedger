#pragma once

#include "phantomledger/activity/spending/actors/event.hpp"
#include "phantomledger/activity/spending/actors/spender.hpp"
#include "phantomledger/activity/spending/market/market.hpp"
#include "phantomledger/activity/spending/routing/channel.hpp"
#include "phantomledger/activity/spending/routing/emission_result.hpp"
#include "phantomledger/primitives/random/rng.hpp"

#include <cstdint>
#include <optional>

namespace PhantomLedger::spending::routing {

class PaymentRouter {
public:
  PaymentRouter(random::Rng &rng, const market::Market &market,
                const PaymentRoutingRules &policy,
                const ResolvedAccounts &resolved) noexcept
      : rng_(rng), market_(market), policy_(policy), resolved_(resolved) {}

  [[nodiscard]] std::optional<EmissionResult> route(Slot slot,
                                                    const actors::Event &event);

  [[nodiscard]] EmissionResult emitBill(const actors::Event &event);
  [[nodiscard]] EmissionResult emitExternal(const actors::Event &event);
  [[nodiscard]] std::optional<EmissionResult>
  emitMerchant(const actors::Event &event);
  [[nodiscard]] std::optional<EmissionResult>
  emitP2p(const actors::Event &event);

private:
  [[nodiscard]] std::uint32_t pickMerchantIndex(const actors::Spender &spender,
                                                double exploreP);

  random::Rng &rng_;
  const market::Market &market_;
  const PaymentRoutingRules &policy_;
  const ResolvedAccounts &resolved_;
};

} // namespace PhantomLedger::spending::routing
