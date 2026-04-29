#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/spending/simulator/config.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/blueprints/models.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"

#include <span>
#include <vector>

namespace PhantomLedger::transfers::legit::routines::spending {

[[nodiscard]] std::vector<transactions::Transaction> generateDayToDayTxns(
    const blueprints::Blueprint &request,
    const blueprints::LegitBuildPlan &plan,
    const entity::account::Ownership &ownership,
    const entity::account::Registry &registry,
    std::span<const transactions::Transaction> baseTxns,
    clearing::Ledger *screenBook = nullptr, bool baseTxnsSorted = false,
    const PhantomLedger::spending::simulator::SimulatorConfig &cfg = {});

} // namespace PhantomLedger::transfers::legit::routines::spending
