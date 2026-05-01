#pragma once

#include "phantomledger/taxonomies/clearing/types.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/lookup.hpp"

#include <array>
#include <optional>
#include <string_view>

namespace PhantomLedger::clearing {

namespace enumTax = ::PhantomLedger::taxonomies::enums;

namespace detail {

static_assert(enumTax::isIndexable(kRejectReasons));
static_assert(enumTax::isIndexable(kProtectionTypes));
static_assert(enumTax::isIndexable(kBankTiers));

inline constexpr auto kRejectReasonEntries =
    std::to_array<lookup::Entry<RejectReason>>({
        {"invalid", RejectReason::invalid},
        {"unbooked", RejectReason::unbooked},
        {"unfunded", RejectReason::unfunded},
    });

inline constexpr auto kProtectionTypeEntries =
    std::to_array<lookup::Entry<ProtectionType>>({
        {"none", ProtectionType::none},
        {"courtesy", ProtectionType::courtesy},
        {"linked", ProtectionType::linked},
        {"loc", ProtectionType::loc},
    });

inline constexpr auto kBankTierEntries =
    std::to_array<lookup::Entry<BankTier>>({
        {"zero_fee", BankTier::zeroFee},
        {"reduced_fee", BankTier::reducedFee},
        {"standard_fee", BankTier::standardFee},
    });

static_assert(kRejectReasonEntries.size() == kRejectReasonCount);
static_assert(kProtectionTypeEntries.size() == kProtectionTypeCount);
static_assert(kBankTierEntries.size() == kBankTierCount);

inline constexpr auto kSortedRejectReasons =
    lookup::sorted(kRejectReasonEntries);

inline constexpr auto kSortedProtectionTypes =
    lookup::sorted(kProtectionTypeEntries);

inline constexpr auto kSortedBankTiers = lookup::sorted(kBankTierEntries);

inline constexpr auto kRejectReasonNames =
    lookup::reverseTableDense<kRejectReasonCount>(
        kRejectReasonEntries,
        [](RejectReason reason) { return enumTax::toIndex(reason); });

inline constexpr auto kProtectionTypeNames =
    lookup::reverseTableDense<kProtectionTypeCount>(
        kProtectionTypeEntries,
        [](ProtectionType type) { return enumTax::toIndex(type); });

inline constexpr auto kBankTierNames =
    lookup::reverseTableDense<kBankTierCount>(
        kBankTierEntries, [](BankTier tier) { return enumTax::toIndex(tier); });

inline constexpr bool kValidated =
    (lookup::requireUniqueNames(kSortedRejectReasons),
     lookup::requireUniqueNames(kSortedProtectionTypes),
     lookup::requireUniqueNames(kSortedBankTiers), true);

} // namespace detail

[[nodiscard]] constexpr std::string_view name(RejectReason reason) noexcept {
  return detail::kRejectReasonNames[enumTax::toIndex(reason)];
}

[[nodiscard]] constexpr std::string_view name(ProtectionType type) noexcept {
  return detail::kProtectionTypeNames[enumTax::toIndex(type)];
}

[[nodiscard]] constexpr std::string_view name(BankTier tier) noexcept {
  return detail::kBankTierNames[enumTax::toIndex(tier)];
}

[[nodiscard]] constexpr std::optional<RejectReason>
parseRejectReason(std::string_view value) noexcept {
  return lookup::find(detail::kSortedRejectReasons, value);
}

[[nodiscard]] constexpr std::optional<ProtectionType>
parseProtectionType(std::string_view value) noexcept {
  return lookup::find(detail::kSortedProtectionTypes, value);
}

[[nodiscard]] constexpr std::optional<BankTier>
parseBankTier(std::string_view value) noexcept {
  return lookup::find(detail::kSortedBankTiers, value);
}

} // namespace PhantomLedger::clearing
