#pragma once

#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/fraud/engine.hpp"
#include "phantomledger/transfers/fraud/rings.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::transfers::fraud::typologies::invoice {

/// Generate invoice-style fraud: fraud accounts paying biller counterparties
/// in business-hour, weekly-cadence amounts to mimic legitimate B2B payments.
[[nodiscard]] std::vector<transactions::Transaction>
generate(IllicitContext &ctx, const Plan &plan, std::int32_t budget);

} // namespace PhantomLedger::transfers::fraud::typologies::invoice
