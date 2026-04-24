#pragma once

#include "phantomledger/taxonomies/lookup.hpp"
#include "phantomledger/taxonomies/merchants/types.hpp"

#include <array>
#include <optional>
#include <string_view>

namespace PhantomLedger::merchants {

inline constexpr std::array<lookup::Entry<Category>, kCategoryCount> kEntries{{
    {"grocery", Category::grocery},
    {"fuel", Category::fuel},
    {"utilities", Category::utilities},
    {"telecom", Category::telecom},
    {"ecommerce", Category::ecommerce},
    {"restaurant", Category::restaurant},
    {"pharmacy", Category::pharmacy},
    {"retail_other", Category::retailOther},
    {"insurance", Category::insurance},
    {"education", Category::education},
}};

inline constexpr auto kSorted = lookup::sorted(kEntries);

inline constexpr auto kNames = lookup::reverseTableDense<kCategoryCount>(
    kEntries, [](Category c) { return slot(c); });

inline constexpr bool kValidated = (lookup::requireUniqueNames(kSorted), true);

[[nodiscard]] constexpr std::string_view name(Category c) noexcept {
  return kNames[slot(c)];
}

[[nodiscard]] constexpr std::optional<Category>
parse(std::string_view s) noexcept {
  return lookup::find(kSorted, s);
}

// Ordered list of all categories — stable for iteration / sampling.
inline constexpr std::array<Category, kCategoryCount> kAll{
    Category::grocery,   Category::fuel,        Category::utilities,
    Category::telecom,   Category::ecommerce,   Category::restaurant,
    Category::pharmacy,  Category::retailOther, Category::insurance,
    Category::education,
};

} // namespace PhantomLedger::merchants
