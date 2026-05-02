#pragma once

#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transfers/legit/blueprints/models.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"

#include <memory>

namespace PhantomLedger::transfers::legit::ledger {

[[nodiscard]] std::unique_ptr<clearing::Ledger>
buildBalanceBook(const blueprints::Timeline &timeline,
                 const blueprints::Network &network,
                 const blueprints::ClearingSpec &specs,
                 const blueprints::CreditCardState &ccState,
                 const blueprints::LegitBuildPlan &plan);

} // namespace PhantomLedger::transfers::legit::ledger
