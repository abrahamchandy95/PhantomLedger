#pragma once

#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/fraud/engine.hpp"
#include "phantomledger/transfers/fraud/rings.hpp"

#include <vector>

namespace PhantomLedger::transfers::fraud::camouflage {

struct Rates {
  double smallP2pPerDayP = 0.03;
  double billMonthlyP = 0.35;
  double salaryInboundP = 0.12;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;

    r.check([&] { v::unit("smallP2pPerDayP", smallP2pPerDayP); });
    r.check([&] { v::unit("billMonthlyP", billMonthlyP); });
    r.check([&] { v::unit("salaryInboundP", salaryInboundP); });
  }
};

/// Generate camouflage transactions for one fraud ring.
[[nodiscard]] std::vector<transactions::Transaction>
generate(CamouflageContext &ctx, const Plan &plan, const Rates &rates = {});

} // namespace PhantomLedger::transfers::fraud::camouflage
