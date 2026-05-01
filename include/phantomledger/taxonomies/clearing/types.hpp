#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace PhantomLedger::clearing {

enum class RejectReason : std::uint8_t {
  invalid = 0,
  unbooked = 1,
  unfunded = 2,
};

enum class ProtectionType : std::uint8_t {
  none = 0,
  courtesy = 1,
  linked = 2,
  loc = 3,
};

enum class BankTier : std::uint8_t {
  zeroFee = 0,
  reducedFee = 1,
  standardFee = 2,
};

inline constexpr auto kRejectReasons = std::to_array<RejectReason>({
    RejectReason::invalid,
    RejectReason::unbooked,
    RejectReason::unfunded,
});

inline constexpr auto kProtectionTypes = std::to_array<ProtectionType>({
    ProtectionType::none,
    ProtectionType::courtesy,
    ProtectionType::linked,
    ProtectionType::loc,
});

inline constexpr auto kBankTiers = std::to_array<BankTier>({
    BankTier::zeroFee,
    BankTier::reducedFee,
    BankTier::standardFee,
});

inline constexpr std::size_t kRejectReasonCount = kRejectReasons.size();
inline constexpr std::size_t kProtectionTypeCount = kProtectionTypes.size();
inline constexpr std::size_t kBankTierCount = kBankTiers.size();

} // namespace PhantomLedger::clearing
