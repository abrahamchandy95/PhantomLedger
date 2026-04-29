#pragma once

#include "phantomledger/transfers/fraud/engine.hpp"

namespace PhantomLedger::transfers::fraud {

[[nodiscard]] InjectionOutput inject(const InjectionInput &request);

} // namespace PhantomLedger::transfers::fraud
