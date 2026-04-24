#pragma once

#include "phantomledger/taxonomies/lookup.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <array>
#include <optional>
#include <string_view>

namespace PhantomLedger::personas {
namespace detail {

// --- Names -------------------------------------------------------

inline constexpr std::array<lookup::Entry<Type>, kKindCount> kEntries{{
    {"student", Type::student},
    {"retired", Type::retiree},
    {"freelancer", Type::freelancer},
    {"smallbiz", Type::smallBusiness},
    {"hnw", Type::highNetWorth},
    {"salaried", Type::salaried},
}};

inline constexpr auto kSorted = lookup::sorted(kEntries);

inline constexpr auto kNames = lookup::reverseTableDense<kKindCount>(
    kEntries, [](Type t) { return slot(t); });

inline constexpr bool kValidated = (lookup::requireUniqueNames(kSorted), true);

inline constexpr std::array<std::string_view, 3> kTimingNames{
    "consumer",
    "consumer_day",
    "business",
};

} // namespace detail

[[nodiscard]] constexpr std::string_view name(Type t) noexcept {
  return detail::kNames[slot(t)];
}

[[nodiscard]] constexpr std::string_view name(Timing t) noexcept {
  return detail::kTimingNames[slot(t)];
}

[[nodiscard]] constexpr std::optional<Type> parse(std::string_view s) noexcept {
  return lookup::find(detail::kSorted, s);
}

[[nodiscard]] constexpr Type parseOr(std::string_view s,
                                     Type fallback = kDefaultType) noexcept {
  if (const auto v = parse(s)) {
    return *v;
  }
  return fallback;
}

[[nodiscard]] constexpr std::optional<Timing>
parseTiming(std::string_view s) noexcept {
  for (std::size_t i = 0; i < detail::kTimingNames.size(); ++i) {
    if (detail::kTimingNames[i] == s) {
      return static_cast<Timing>(i);
    }
  }
  return std::nullopt;
}

} // namespace PhantomLedger::personas
