#pragma once

#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/lookup.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace PhantomLedger::personas {

namespace enumTax = ::PhantomLedger::taxonomies::enums;

namespace detail {

static_assert(enumTax::isIndexable(kTypes));
static_assert(enumTax::isIndexable(kTimings));

inline constexpr auto kTypeEntries = std::to_array<lookup::Entry<Type>>({
    {"student", Type::student},
    {"retired", Type::retiree},
    {"freelancer", Type::freelancer},
    {"smallbiz", Type::smallBusiness},
    {"hnw", Type::highNetWorth},
    {"salaried", Type::salaried},
});

inline constexpr auto kTimingEntries = std::to_array<lookup::Entry<Timing>>({
    {"consumer", Timing::consumer},
    {"consumer_day", Timing::consumerDay},
    {"business", Timing::business},
});

static_assert(kTypeEntries.size() == kTypeCount);
static_assert(kTimingEntries.size() == kTimingCount);

inline constexpr auto kSortedTypes = lookup::sorted(kTypeEntries);
inline constexpr auto kSortedTimings = lookup::sorted(kTimingEntries);

inline constexpr auto kTypeNames = lookup::reverseTableDense<kTypeCount>(
    kTypeEntries, [](Type type) { return enumTax::toIndex(type); });

inline constexpr auto kTimingNames = lookup::reverseTableDense<kTimingCount>(
    kTimingEntries, [](Timing timing) { return enumTax::toIndex(timing); });

inline constexpr bool kValidated =
    (lookup::requireUniqueNames(kSortedTypes),
     lookup::requireUniqueNames(kSortedTimings), true);

} // namespace detail

[[nodiscard]] constexpr std::string_view name(Type type) noexcept {
  return detail::kTypeNames[enumTax::toIndex(type)];
}

[[nodiscard]] constexpr std::string_view name(Timing timing) noexcept {
  return detail::kTimingNames[enumTax::toIndex(timing)];
}

[[nodiscard]] constexpr std::optional<Type>
parse(std::string_view value) noexcept {
  return lookup::find(detail::kSortedTypes, value);
}

[[nodiscard]] constexpr Type parseOr(std::string_view value,
                                     Type fallback = kDefaultType) noexcept {
  if (const auto parsed = parse(value)) {
    return *parsed;
  }

  return fallback;
}

[[nodiscard]] constexpr std::optional<Timing>
parseTiming(std::string_view value) noexcept {
  return lookup::find(detail::kSortedTimings, value);
}

} // namespace PhantomLedger::personas
