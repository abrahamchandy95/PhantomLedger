#pragma once

#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transfers/legit/blueprints/models.hpp"

namespace PhantomLedger::transfers::family {
struct GraphConfig;
struct TransferConfig;
} // namespace PhantomLedger::transfers::family

namespace PhantomLedger::transfers::legit::ledger {

struct LegitTransferBuilder {
  const blueprints::Blueprint *request = nullptr;
  const family::GraphConfig *famGraphCfg = nullptr;
  const family::TransferConfig *famTransferCfg = nullptr;

  [[nodiscard]] blueprints::TransfersPayload build() const;
};

} // namespace PhantomLedger::transfers::legit::ledger
