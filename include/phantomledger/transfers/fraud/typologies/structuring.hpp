#pragma once

#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/fraud/engine.hpp"
#include "phantomledger/transfers/fraud/rings.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::transfers::fraud::typologies::structuring {

struct Rules {
  double threshold = 10'000.0;
  double epsilonMin = 50.0;
  double epsilonMax = 400.0;
  std::int32_t splitsMin = 3;
  std::int32_t splitsMax = 12;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;

    r.check([&] { v::positive("threshold", threshold); });
    r.check([&] { v::ge("epsilonMax", epsilonMax, epsilonMin); });
    r.check([&] { v::ge("splitsMin", splitsMin, 1); });
    r.check([&] { v::ge("splitsMax", splitsMax, splitsMin); });
  }
};

[[nodiscard]] std::vector<transactions::Transaction>
generate(IllicitContext &ctx, const Plan &plan, std::int32_t budget,
         const Rules &rules = {});

} // namespace PhantomLedger::transfers::fraud::typologies::structuring
