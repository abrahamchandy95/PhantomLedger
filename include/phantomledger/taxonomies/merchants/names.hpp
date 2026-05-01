#pragma once

#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/lookup.hpp"
#include "phantomledger/taxonomies/merchants/types.hpp"

#include <array>
#include <optional>
#include <string_view>

namespace PhantomLedger::merchants {

using namespace ::PhantomLedger::taxonomies::enums;

namespace detail {

inline constexpr auto kEntries = std::to_array<lookup::Entry<Category>>({
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
});

static_assert(isIndexable(kCategories));
static_assert(kEntries.size() == kCategoryCount);

inline constexpr auto kSorted = lookup::sorted(kEntries);

inline constexpr auto kNames = lookup::reverseTableDense<kCategoryCount>(
    kEntries, [](Category category) { return toIndex(category); });

inline constexpr bool kValidated = (lookup::requireUniqueNames(kSorted), true);

} // namespace detail

[[nodiscard]] constexpr std::string_view name(Category category) noexcept {
  return detail::kNames[toIndex(category)];
}

[[nodiscard]] constexpr std::optional<Category>
parse(std::string_view value) noexcept {
  return lookup::find(detail::kSorted, value);
}

} // namespace PhantomLedger::merchants
