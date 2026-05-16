#pragma once

#include "phantomledger/activity/spending/dynamics/dormancy/state.hpp"
#include "phantomledger/activity/spending/dynamics/momentum/state.hpp"
#include "phantomledger/activity/spending/dynamics/paycheck/boost.hpp"

namespace PhantomLedger::activity::spending::dynamics::population {

struct PersonDynamics {
  momentum::State momentum{};
  dormancy::State dormancy{};
  paycheck::State paycheck{};
};

} // namespace PhantomLedger::activity::spending::dynamics::population
