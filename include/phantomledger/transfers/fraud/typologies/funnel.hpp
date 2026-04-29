#pragma once

#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/fraud/engine.hpp"
#include "phantomledger/transfers/fraud/rings.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::transfers::fraud::typologies::funnel {

[[nodiscard]] std::vector<transactions::Transaction>
generate(IllicitContext &ctx, const Plan &plan, std::int32_t budget);

} // namespace PhantomLedger::transfers::fraud::typologies::funnel
