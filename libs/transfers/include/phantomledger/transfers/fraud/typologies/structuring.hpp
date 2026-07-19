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
  // fraud-audit-2026-07 F1: ε ∈ [50, 1500] puts threshold-profile splits
  // in $8,500–$9,950, so ≈⅓ of them fall BELOW the [9,000, 10,000) alert
  // band — the alert label is realistically incomplete by design.
  double epsilonMax = 1'500.0;
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
