#pragma once

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

namespace PhantomLedger::transfers::legit::routines::spending {

struct SpendingHabits {
  ::PhantomLedger::spending::config::MerchantPickRules picking =
      ::PhantomLedger::spending::config::kDefaultPickRules;

  ::PhantomLedger::spending::config::ExplorationHabits exploration =
      ::PhantomLedger::spending::config::kDefaultExploration;

  ::PhantomLedger::spending::config::BurstBehavior burst =
      ::PhantomLedger::spending::config::kDefaultBurst;
};

inline constexpr SpendingHabits kDefaultSpendingHabits{};

struct RunPlanning {
  ::PhantomLedger::spending::simulator::TransactionLoad load{};
  ::PhantomLedger::spending::routing::ChannelWeights channels{};
  ::PhantomLedger::spending::routing::PaymentRoutingRules paymentRules{};
};

inline constexpr RunPlanning kDefaultRunPlanning{};

struct DayPattern {
  ::PhantomLedger::spending::simulator::DaySource::Variation variation{};

  ::PhantomLedger::math::seasonal::Config seasonal =
      ::PhantomLedger::math::seasonal::kDefaultConfig;
};

inline constexpr DayPattern kDefaultDayPattern{};

struct EmissionProfile {
  double baseExploreP = 0.0;

  ::PhantomLedger::spending::config::LiquidityConstraints liquidity =
      ::PhantomLedger::spending::config::kDefaultLiquidityConstraints;
};

inline constexpr EmissionProfile kDefaultEmissionProfile{};

struct DynamicsProfile {
  ::PhantomLedger::spending::dynamics::Config value =
      ::PhantomLedger::spending::dynamics::kDefaultConfig;
};

inline constexpr DynamicsProfile kDefaultDynamicsProfile{};

} // namespace PhantomLedger::transfers::legit::routines::spending
