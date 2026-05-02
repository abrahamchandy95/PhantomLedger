#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/math/seasonal.hpp"
#include "phantomledger/spending/config/burst.hpp"
#include "phantomledger/spending/config/exploration.hpp"
#include "phantomledger/spending/config/liquidity.hpp"
#include "phantomledger/spending/config/picking.hpp"
#include "phantomledger/spending/dynamics/config.hpp"
#include "phantomledger/spending/routing/channel.hpp"
#include "phantomledger/spending/routing/payments.hpp"
#include "phantomledger/spending/simulator/day_source.hpp"
#include "phantomledger/spending/simulator/plan.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/blueprints/models.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"

#include <span>
#include <vector>

namespace PhantomLedger::transfers::legit::routines::spending {

namespace plConfig = ::PhantomLedger::spending::config;
namespace plDynamics = ::PhantomLedger::spending::dynamics;
namespace plRouting = ::PhantomLedger::spending::routing;
namespace plSeasonal = ::PhantomLedger::math::seasonal;
namespace plSimulator = ::PhantomLedger::spending::simulator;

[[nodiscard]] std::vector<transactions::Transaction> generateDayToDayTxns(
    const blueprints::Blueprint &request,
    const blueprints::LegitBuildPlan &plan,
    const entity::account::Ownership &ownership,
    const entity::account::Registry &registry,
    std::span<const transactions::Transaction> baseTxns,
    clearing::Ledger *screenBook = nullptr, bool baseTxnsSorted = false,

    plConfig::MerchantPickRules picking = plConfig::kDefaultPickRules,
    plConfig::ExplorationHabits exploration = plConfig::kDefaultExploration,
    plConfig::BurstBehavior burst = plConfig::kDefaultBurst,

    plSimulator::TransactionLoad load = {},
    plRouting::ChannelWeights channels = {},
    plRouting::PaymentRoutingRules paymentRules = {}, double baseExploreP = 0.0,
    plSimulator::DaySource::Variation day = plSimulator::DaySource::Variation{},
    plConfig::LiquidityConstraints liquidity =
        plConfig::kDefaultLiquidityConstraints,
    plDynamics::Config dynamics = plDynamics::kDefaultConfig,
    plSeasonal::Config seasonal = plSeasonal::kDefaultConfig);

} // namespace PhantomLedger::transfers::legit::routines::spending
