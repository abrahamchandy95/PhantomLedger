#pragma once

#include "phantomledger/activity/spending/actors/explore.hpp"
#include "phantomledger/activity/spending/dynamics/population/advance.hpp"
#include "phantomledger/activity/spending/liquidity/multiplier.hpp"
#include "phantomledger/activity/spending/market/bootstrap.hpp"
#include "phantomledger/activity/spending/routing/channel.hpp"
#include "phantomledger/activity/spending/simulator/day_source.hpp"
#include "phantomledger/activity/spending/simulator/run_planner.hpp"
#include "phantomledger/math/counts.hpp"
#include "phantomledger/math/evolution.hpp"
#include "phantomledger/math/seasonal.hpp"

namespace PhantomLedger::transfers::legit::routines::spending {

struct SpendingHabits {
  ::PhantomLedger::activity::spending::market::MerchantPickBudget picking{};
  ::PhantomLedger::activity::spending::market::ExplorationDistribution exploration{};
  ::PhantomLedger::activity::spending::market::BurstSchedule burst{};
};

inline constexpr SpendingHabits kDefaultSpendingHabits{};

struct RunPlanning {
  ::PhantomLedger::activity::spending::simulator::RunPlanner::TransactionLoad load{};
  ::PhantomLedger::activity::spending::routing::ChannelWeights channels{};
  ::PhantomLedger::activity::spending::routing::PaymentRoutingRules paymentRules{};
};

inline constexpr RunPlanning kDefaultRunPlanning{};

struct DayPattern {
  ::PhantomLedger::activity::spending::simulator::DaySource::Variation variation{};

  ::PhantomLedger::math::seasonal::Config seasonal =
      ::PhantomLedger::math::seasonal::kDefaultConfig;
};

inline constexpr DayPattern kDefaultDayPattern{};

struct EmissionProfile {
  double baseExploreP = 0.02;

  ::PhantomLedger::activity::spending::actors::ExploreModifiers exploration{};
  ::PhantomLedger::activity::spending::liquidity::Throttle liquidity{};
  ::PhantomLedger::math::counts::Rates rates{};
};

inline constexpr EmissionProfile kDefaultEmissionProfile{};

struct DynamicsProfile {
  ::PhantomLedger::activity::spending::dynamics::population::Drivers population{};
  ::PhantomLedger::math::evolution::Config commerce{};
};

inline constexpr DynamicsProfile kDefaultDynamicsProfile{};

} // namespace PhantomLedger::transfers::legit::routines::spending
