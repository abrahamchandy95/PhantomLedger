#pragma once

#include "phantomledger/spending/market/market.hpp"
#include "phantomledger/spending/obligations/snapshot.hpp"
#include "phantomledger/spending/routing/channel.hpp"
#include "phantomledger/spending/routing/payments.hpp"
#include "phantomledger/spending/simulator/plan.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"

#include <span>

namespace PhantomLedger::spending::simulator {

class RunPlanner {
public:
  RunPlanner(TransactionLoad load = {}, routing::ChannelWeights channels = {},
             routing::PaymentRoutingRules paymentRules = {});

  [[nodiscard]] const TransactionLoad &load() const noexcept { return load_; }

  [[nodiscard]] RunPlan build(const market::Market &market,
                              const obligations::Snapshot &obligations,
                              const clearing::Ledger *ledger,
                              std::span<const double> sensitivities) const;

private:
  TransactionLoad load_{};
  routing::ChannelWeights channels_{};
  routing::PaymentRoutingRules paymentRules_{};
};

} // namespace PhantomLedger::spending::simulator
