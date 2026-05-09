#pragma once

#include "phantomledger/activity/spending/actors/explore.hpp"
#include "phantomledger/activity/spending/dynamics/population/advance.hpp"
#include "phantomledger/activity/spending/liquidity/multiplier.hpp"
#include "phantomledger/activity/spending/market/bootstrap.hpp"
#include "phantomledger/activity/spending/routing/channel.hpp"
#include "phantomledger/activity/spending/simulator/day_source.hpp"
#include "phantomledger/activity/spending/simulator/run_planner.hpp"
#include "phantomledger/math/evolution.hpp"
#include "phantomledger/math/seasonal.hpp"

namespace PhantomLedger::transfers::legit::routines::spending {

struct SpendingHabits {
  ::PhantomLedger::spending::market::MerchantPickBudget picking{};
  ::PhantomLedger::spending::market::ExplorationDistribution exploration{};
  ::PhantomLedger::spending::market::BurstSchedule burst{};
};

inline constexpr SpendingHabits kDefaultSpendingHabits{};

struct RunPlanning {
  ::PhantomLedger::spending::simulator::RunPlanner::TransactionLoad load{};
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
  ::PhantomLedger::spending::actors::ExploreModifiers exploration{};
  ::PhantomLedger::spending::liquidity::Throttle liquidity{};
};

inline constexpr EmissionProfile kDefaultEmissionProfile{};

struct DynamicsProfile {
  ::PhantomLedger::spending::dynamics::population::Drivers population{};
  ::PhantomLedger::math::evolution::Config commerce{};
};

inline constexpr DynamicsProfile kDefaultDynamicsProfile{};

} // namespace PhantomLedger::transfers::legit::routines::spending
