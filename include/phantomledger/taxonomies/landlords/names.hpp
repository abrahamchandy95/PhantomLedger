#pragma once

#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/landlords/types.hpp"
#include "phantomledger/taxonomies/lookup.hpp"

#include <array>
#include <optional>
#include <string_view>

namespace PhantomLedger::landlords {

using namespace ::PhantomLedger::taxonomies::enums;

namespace detail {

static_assert(isIndexable(kTypes));

inline constexpr auto kEntries = std::to_array<lookup::Entry<Type>>({
    {"individual", Type::individual},
    {"small_llc", Type::llcSmall},
    {"corporate", Type::corporate},
});

static_assert(kEntries.size() == kTypeCount);

inline constexpr auto kSorted = lookup::sorted(kEntries);

inline constexpr auto kNames = lookup::reverseTableDense<kTypeCount>(
    kEntries, [](Type type) { return toIndex(type); });

inline constexpr bool kValidated = (lookup::requireUniqueNames(kSorted), true);

} // namespace detail

[[nodiscard]] constexpr std::string_view name(Type type) noexcept {
  return detail::kNames[toIndex(type)];
}

[[nodiscard]] constexpr std::optional<Type>
parse(std::string_view value) noexcept {
  return lookup::find(detail::kSorted, value);
}

} // namespace PhantomLedger::landlords
