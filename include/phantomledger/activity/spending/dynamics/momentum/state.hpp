#pragma once

#include "phantomledger/math/momentus.hpp"

namespace PhantomLedger::spending::dynamics::momentum {

using State = math::momentum::State;
using Config = math::momentum::Config;

inline constexpr Config kDefaultConfig = math::momentum::kDefaultConfig;

} // namespace PhantomLedger::spending::dynamics::momentum
