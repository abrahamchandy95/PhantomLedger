#pragma once

#include "phantomledger/math/dormancy.hpp"

namespace PhantomLedger::spending::dynamics::dormancy {

using State = math::dormancy::State;
using Config = math::dormancy::Config;
using Phase = math::dormancy::Phase;

inline constexpr Config kDefaultConfig = math::dormancy::kDefaultConfig;

} // namespace PhantomLedger::spending::dynamics::dormancy
