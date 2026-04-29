#pragma once

#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/blueprints/models.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"

#include <vector>

namespace PhantomLedger::transfers::family {
struct GraphConfig;
struct TransferConfig;
} // namespace PhantomLedger::transfers::family

namespace PhantomLedger::transfers::legit::routines::relatives {

[[nodiscard]] std::vector<transactions::Transaction> generateFamilyTxns(
    const blueprints::Blueprint &request, const family::GraphConfig &graphCfg,
    const family::TransferConfig &transferCfg,
    const blueprints::LegitBuildPlan &plan, const transactions::Factory &txf);

} // namespace PhantomLedger::transfers::legit::routines::relatives
