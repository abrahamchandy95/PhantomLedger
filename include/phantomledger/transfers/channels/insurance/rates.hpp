#pragma once

#include "phantomledger/primitives/validate/checks.hpp"

namespace PhantomLedger::transfers::insurance {

struct ClaimPayout {
  double median = 0.0;
  double sigma = 0.0;
  double floor = 0.0;
};

struct ClaimRates {
  double autoMedian = 4700.0;
  double autoSigma = 0.80;
  double autoFloor = 500.0;
  // Implied mean ~$17.2k against the III/ISO average home claim
  // severity ~$17-18k — the old LN($15,750, .90) implied ~$23.6k
  // (household-econ-2026-07; docs/fraud_model_audit.md L-8).
  double homeMedian = 12500.0;
  double homeSigma = 0.80;
  double homeFloor = 1000.0;

  [[nodiscard]] constexpr ClaimPayout autoPayout() const noexcept {
    return ClaimPayout{
        .median = autoMedian,
        .sigma = autoSigma,
        .floor = autoFloor,
    };
  }

  [[nodiscard]] constexpr ClaimPayout homePayout() const noexcept {
    return ClaimPayout{
        .median = homeMedian,
        .sigma = homeSigma,
        .floor = homeFloor,
    };
  }

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
