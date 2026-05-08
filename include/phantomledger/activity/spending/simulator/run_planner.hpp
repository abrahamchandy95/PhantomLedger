#pragma once

#include "phantomledger/activity/spending/market/market.hpp"
#include "phantomledger/activity/spending/obligations/snapshot.hpp"
#include "phantomledger/activity/spending/routing/channel.hpp"
#include "phantomledger/activity/spending/routing/payments.hpp"
#include "phantomledger/activity/spending/simulator/prepared_run.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"

#include <cstdint>
#include <span>

namespace PhantomLedger::spending::simulator {

class RunPlanner {
public:
  struct TransactionLoad {
    double txnsPerMonth = 0.0;
    std::uint32_t personDailyLimit = 0;
  };

  RunPlanner() = default;

  RunPlanner(TransactionLoad transactionLoad,
             routing::ChannelWeights channels = {},
             routing::PaymentRoutingRules paymentRules = {});

  [[nodiscard]] double txnsPerMonth() const noexcept {
    return transactionLoad_.txnsPerMonth;
  }

  [[nodiscard]] PreparedRun build(const market::Market &market,
                                  const obligations::Snapshot &obligations,
                                  const clearing::Ledger *ledger,
                                  std::span<const double> sensitivities) const;

private:
  TransactionLoad transactionLoad_{};
  routing::ChannelWeights channels_{};
  routing::PaymentRoutingRules paymentRules_{};
};

} // namespace PhantomLedger::spending::simulator
