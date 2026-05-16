#pragma once

#include "phantomledger/math/momentum.hpp"

namespace PhantomLedger::activity::spending::dynamics::momentum {

using State = math::momentum::State;
using Config = math::momentum::Config;

inline constexpr Config kDefaultConfig = math::momentum::kDefaultConfig;

} // namespace PhantomLedger::activity::spending::dynamics::momentum
