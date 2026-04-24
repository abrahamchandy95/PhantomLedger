#pragma once
/*
 * Protection type, bank tier, and LOC configuration.
 *
 * Each internal account has exactly one ProtectionType (NONE, COURTESY,
 * LINKED, or LOC). The ledger enforces this by storing the type and
 * routing funding checks / fee emission based on it.
 *
 * Bank tier is an independent attribute controlling overdraft fee size.
 * Tier distribution matches a typical US retail bank's published fee
 * disclosures (small-bank and basic-checking ~15% zero-fee, promotional
 * ~10% reduced, mainstream ~75% standard).
 */

#include <array>
#include <cstddef>
#include <cstdint>

namespace PhantomLedger::clearing {

/// Mutually exclusive per account. Exactly one at any time.
enum class ProtectionType : std::uint8_t {
  none = 0,
  courtesy = 1, // courtesy cushion; single flat fee when tripped
  linked = 2,   // linked-account sweep; single flat fee when tripped
  loc = 3,      // line of credit; accrues APR interest, no flat fee
};

/// Independent of protection type. Controls overdraft fee size.
enum class BankTier : std::uint8_t {
  zeroFee = 0,     // ~15% of accounts, $0 fee
  reducedFee = 1,  // ~10% of accounts, ~$15 median
  standardFee = 2, // ~75% of accounts, ~$35 median
};

inline constexpr std::size_t kProtectionTypeCount = 4;
inline constexpr std::size_t kBankTierCount = 3;

/// Tier population distribution. Sums to 1.0.
struct TierWeights {
  double zeroFee = 0.15;
  double reducedFee = 0.10;
  double standardFee = 0.75;

  [[nodiscard]] constexpr double total() const noexcept {
    return zeroFee + reducedFee + standardFee;
  }
};

/// Per-tier overdraft fee amount distribution (lognormal by median).
struct TierFeeProfile {
  double median;
  double sigma;
};

inline constexpr std::array<TierFeeProfile, kBankTierCount> kTierFeeProfiles{{
    {0.0, 0.0},    // zeroFee
    {15.00, 0.25}, // reducedFee: median $15
    {35.00, 0.20}, // standardFee: median $35
}};

[[nodiscard]] constexpr TierFeeProfile tierFeeProfile(BankTier tier) noexcept {
  return kTierFeeProfiles[static_cast<std::size_t>(tier)];
}

/// Default parameters for line-of-credit accounts.
struct LocDefaults {
  double aprMean = 0.18;  // 18% APR
  double aprSigma = 0.04; // small spread around the mean
  int billingDayMin = 1;  // day of month
  int billingDayMax = 28; // day of month, inclusive
};

/// Per-persona protection-type weights. The protection sampling is a
/// single categorical draw: courtesy / linked / loc / none, with none
/// taking whatever probability remains after the other three.
///
/// Mutual exclusivity is a hard invariant. The previous balance_book
/// sampled each protection with an independent coin flip, which could
/// produce accounts carrying all three simultaneously. The new sampler
/// uses a single 4-way CDF draw.
struct PersonaProtectionShares {
  double courtesy;
  double linked;
  double loc;

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
