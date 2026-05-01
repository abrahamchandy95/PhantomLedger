#pragma once

#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/lookup.hpp"
#include "phantomledger/taxonomies/products/types.hpp"

#include <array>
#include <optional>
#include <string_view>

namespace PhantomLedger::products {

namespace enumTax = ::PhantomLedger::taxonomies::enums;

namespace detail {

static_assert(enumTax::isIndexable(kDirections));
static_assert(enumTax::isIndexable(kProductTypes));
static_assert(enumTax::isIndexable(kPolicyTypes));

inline constexpr auto kDirectionEntries =
    std::to_array<lookup::Entry<Direction>>({
        {"outflow", Direction::outflow},
        {"inflow", Direction::inflow},
    });

inline constexpr auto kProductTypeEntries =
    std::to_array<lookup::Entry<ProductType>>({
        {"unknown", ProductType::unknown},
        {"mortgage", ProductType::mortgage},
        {"auto_loan", ProductType::autoLoan},
        {"student_loan", ProductType::studentLoan},
        {"insurance", ProductType::insurance},
        {"tax", ProductType::tax},
    });

inline constexpr auto kPolicyTypeEntries =
    std::to_array<lookup::Entry<PolicyType>>({
        {"auto", PolicyType::auto_},
        {"home", PolicyType::home},
        {"life", PolicyType::life},
    });

static_assert(kDirectionEntries.size() == kDirectionCount);
static_assert(kProductTypeEntries.size() == kProductTypeCount);
static_assert(kPolicyTypeEntries.size() == kPolicyTypeCount);

inline constexpr auto kSortedDirections = lookup::sorted(kDirectionEntries);
inline constexpr auto kSortedProductTypes = lookup::sorted(kProductTypeEntries);
inline constexpr auto kSortedPolicyTypes = lookup::sorted(kPolicyTypeEntries);

inline constexpr auto kDirectionNames =
    lookup::reverseTableDense<kDirectionCount>(
        kDirectionEntries,
        [](Direction direction) { return enumTax::toIndex(direction); });

inline constexpr auto kProductTypeNames =
    lookup::reverseTableDense<kProductTypeCount>(
        kProductTypeEntries,
        [](ProductType type) { return enumTax::toIndex(type); });

inline constexpr auto kPolicyTypeNames =
    lookup::reverseTableDense<kPolicyTypeCount>(
        kPolicyTypeEntries,
        [](PolicyType type) { return enumTax::toIndex(type); });

inline constexpr bool kValidated =
    (lookup::requireUniqueNames(kSortedDirections),
     lookup::requireUniqueNames(kSortedProductTypes),
     lookup::requireUniqueNames(kSortedPolicyTypes), true);

} // namespace detail

[[nodiscard]] constexpr std::string_view name(Direction direction) noexcept {
  return detail::kDirectionNames[enumTax::toIndex(direction)];
}

[[nodiscard]] constexpr std::string_view name(ProductType type) noexcept {
  return detail::kProductTypeNames[enumTax::toIndex(type)];
}

[[nodiscard]] constexpr std::string_view name(PolicyType type) noexcept {
  return detail::kPolicyTypeNames[enumTax::toIndex(type)];
}

[[nodiscard]] constexpr std::optional<Direction>
parseDirection(std::string_view value) noexcept {
  return lookup::find(detail::kSortedDirections, value);
}

[[nodiscard]] constexpr std::optional<ProductType>
parseProductType(std::string_view value) noexcept {
  return lookup::find(detail::kSortedProductTypes, value);
}

[[nodiscard]] constexpr std::optional<PolicyType>
parsePolicyType(std::string_view value) noexcept {
  return lookup::find(detail::kSortedPolicyTypes, value);
}

} // namespace PhantomLedger::products
