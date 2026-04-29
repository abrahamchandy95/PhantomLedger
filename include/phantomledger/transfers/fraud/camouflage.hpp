#pragma once

#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/fraud/engine.hpp"
#include "phantomledger/transfers/fraud/rings.hpp"

#include <vector>

namespace PhantomLedger::transfers::fraud::camouflage {

/// Generate camouflage transactions for one fraud ring.
[[nodiscard]] std::vector<transactions::Transaction>
generate(CamouflageContext &ctx, const Plan &plan);

} // namespace PhantomLedger::transfers::fraud::camouflage
