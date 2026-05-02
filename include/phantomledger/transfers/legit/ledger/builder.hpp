#pragma once

#include "phantomledger/transfers/legit/blueprints/plans.hpp"
#include "phantomledger/transfers/legit/ledger/limits.hpp"
#include "phantomledger/transfers/legit/ledger/passes.hpp"
#include "phantomledger/transfers/legit/ledger/result.hpp"

namespace PhantomLedger::infra {
class Router;
} // namespace PhantomLedger::infra

namespace PhantomLedger::transfers::legit::ledger {

struct LegitTransferRequest {
  blueprints::PlanRequest plan{};
  BalanceBookRequest balanceBook{};

  passes::IncomePassRequest income{};
  passes::RoutinePassRequest routines{};
  passes::FamilyPassRequest family{};
  passes::CreditPassRequest credit{};

  const ::PhantomLedger::transfers::family::GraphConfig *famGraphCfg = nullptr;
  const ::PhantomLedger::transfers::legit::routines::relatives::
      FamilyTransferModel *familyTransfers = nullptr;

  const ::PhantomLedger::infra::Router *router = nullptr;
};

struct LegitTransferBuilder {
  const LegitTransferRequest *request = nullptr;

  [[nodiscard]] TransfersPayload build() const;
};

} // namespace PhantomLedger::transfers::legit::ledger
