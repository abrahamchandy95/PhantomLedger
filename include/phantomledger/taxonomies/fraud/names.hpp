#pragma once

#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/fraud/types.hpp"
#include "phantomledger/taxonomies/lookup.hpp"

#include <array>
#include <optional>
#include <string_view>

namespace PhantomLedger::fraud {

namespace enumTax = ::PhantomLedger::taxonomies::enums;

namespace detail {

static_assert(enumTax::isIndexable(kTypologies));

inline constexpr auto kTypologyEntries =
    std::to_array<lookup::Entry<Typology>>({
        {"classic", Typology::classic},
        {"layering", Typology::layering},
        {"funnel", Typology::funnel},
        {"structuring", Typology::structuring},
        {"invoice", Typology::invoice},
        {"mule", Typology::mule},
    });

static_assert(kTypologyEntries.size() == kTypologyCount);

inline constexpr auto kSortedTypologies = lookup::sorted(kTypologyEntries);

inline constexpr auto kTypologyNames =
    lookup::reverseTableDense<kTypologyCount>(
        kTypologyEntries,
        [](Typology typology) { return enumTax::toIndex(typology); });

inline constexpr bool kValidated =
    (lookup::requireUniqueNames(kSortedTypologies), true);

} // namespace detail

[[nodiscard]] constexpr std::string_view name(Typology typology) noexcept {
  return detail::kTypologyNames[enumTax::toIndex(typology)];
}

[[nodiscard]] constexpr std::optional<Typology>
parseTypology(std::string_view value) noexcept {
  return lookup::find(detail::kSortedTypologies, value);
}

} // namespace PhantomLedger::fraud
