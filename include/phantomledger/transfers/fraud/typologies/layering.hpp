#pragma once

#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/fraud/engine.hpp"
#include "phantomledger/transfers/fraud/rings.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::transfers::fraud::typologies::layering {

struct Rules {
  std::int32_t minHops = 3;
  std::int32_t maxHops = 8;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;

    r.check([&] { v::ge("minHops", minHops, 1); });
    r.check([&] { v::ge("maxHops", maxHops, minHops); });
  }
};

[[nodiscard]] std::vector<transactions::Transaction>
generate(IllicitContext &ctx, const Plan &plan, std::int32_t budget,
         const Rules &rules = {});

} // namespace PhantomLedger::transfers::fraud::typologies::layering
