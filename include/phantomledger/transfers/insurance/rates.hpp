#pragma once
/*
 * Insurance claim payout distributions.
 *
 * Premium amounts are not stored here — they live on the
 * InsurancePolicy records inside the portfolio, set at policy
 * construction time by the product builder. The transfer layer reads
 * `policy.monthlyPremium` directly.
 * Auto and home are the only insurance types that produce claim
 * events; life is excluded because death events are not modeled.
 */

#include "phantomledger/primitives/validate/checks.hpp"

namespace PhantomLedger::transfers::insurance {

struct ClaimRates {
  // Auto claims. Median and sigma drive the lognormal-by-median; floor
  // is the minimum dollar amount enforced after sampling.
  double autoMedian = 4700.0;
  double autoSigma = 0.80;
  double autoFloor = 500.0;

  // Home claims. Higher floor reflects the smaller universe of trivial
  // home claims that show up as separate transactions.
  double homeMedian = 15750.0;
  double homeSigma = 0.90;
  double homeFloor = 1000.0;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::positive("autoMedian", autoMedian); });
    r.check([&] { v::nonNegative("autoSigma", autoSigma); });
    r.check([&] { v::nonNegative("autoFloor", autoFloor); });
    r.check([&] { v::positive("homeMedian", homeMedian); });
    r.check([&] { v::nonNegative("homeSigma", homeSigma); });
    r.check([&] { v::nonNegative("homeFloor", homeFloor); });
  }
};

} // namespace PhantomLedger::transfers::insurance
