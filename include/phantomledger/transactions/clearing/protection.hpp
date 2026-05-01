#pragma once

#include "phantomledger/taxonomies/clearing/types.hpp"

namespace PhantomLedger::clearing {

/// Tier population distribution. Sums to 1.0.
struct TierWeights {
  double zeroFee = 0.15;
  double reducedFee = 0.10;
  double standardFee = 0.75;

  [[nodiscard]] constexpr double total() const noexcept {
    return zeroFee + reducedFee + standardFee;
  }

  [[nodiscard]] constexpr bool valid() const noexcept {
    return zeroFee >= 0.0 && reducedFee >= 0.0 && standardFee >= 0.0 &&
           total() > 0.0;
  }
};

/// Per-tier overdraft fee amount distribution.
struct TierFeeProfile {
  double median = 0.0;
  double sigma = 0.0;
};

[[nodiscard]] constexpr TierFeeProfile tierFeeProfile(BankTier tier) noexcept {
  switch (tier) {
  case BankTier::zeroFee:
    return {.median = 0.0, .sigma = 0.0};

  case BankTier::reducedFee:
    return {.median = 15.00, .sigma = 0.25};

  case BankTier::standardFee:
    return {.median = 35.00, .sigma = 0.20};
  }

  return {};
}

/// Default parameters for line-of-credit accounts.
struct LocDefaults {
  double aprMean = 0.18;
  double aprSigma = 0.04;
  int billingDayMin = 1;
  int billingDayMax = 28;
};

struct PersonaProtectionShares {
  double courtesy = 0.0;
  double linked = 0.0;
  double loc = 0.0;

  [[nodiscard]] constexpr double none() const noexcept {
    const double assigned = courtesy + linked + loc;
    return assigned >= 1.0 ? 0.0 : 1.0 - assigned;
  }

  [[nodiscard]] constexpr bool valid() const noexcept {
    return courtesy >= 0.0 && linked >= 0.0 && loc >= 0.0 &&
           (courtesy + linked + loc) <= 1.0 + 1e-9;
  }
};

} // namespace PhantomLedger::clearing
