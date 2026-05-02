#pragma once

#include "phantomledger/transfers/legit/blueprints/models.hpp"

namespace PhantomLedger::transfers::family {
struct GraphConfig;
} // namespace PhantomLedger::transfers::family

namespace PhantomLedger::transfers::legit::routines::relatives {
struct FamilyTransferModel;
} // namespace PhantomLedger::transfers::legit::routines::relatives
  //
namespace PhantomLedger::transfers::legit::ledger {

struct LegitTransferBuilder {
  const blueprints::Blueprint *request = nullptr;
  const family::GraphConfig *famGraphCfg = nullptr;
  const routines::relatives::FamilyTransferModel *familyTransfers = nullptr;

  [[nodiscard]] blueprints::TransfersPayload build() const;
};

} // namespace PhantomLedger::transfers::legit::ledger
