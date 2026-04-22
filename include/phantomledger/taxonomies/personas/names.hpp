#pragma once

#include "phantomledger/taxonomies/personas/types.hpp"

#include <array>
#include <optional>
#include <string_view>

namespace PhantomLedger::personas {
namespace detail {

struct NamedKind {
  std::string_view name;
  Type type;
};

inline constexpr std::array<NamedKind, kKindCount> kNamedKinds{{
    {"student", Type::student},
    {"retired", Type::retiree},
    {"freelancer", Type::freelancer},
    {"smallbiz", Type::smallBusiness},
    {"hnw", Type::highNetWorth},
    {"salaried", Type::salaried},
}};

template <std::size_t N>
[[nodiscard]] consteval std::array<NamedKind, N>
sortByName(std::array<NamedKind, N> arr) {
  for (std::size_t i = 1; i < N; ++i) {
    auto key = arr[i];
    std::size_t j = i;
    while (j > 0 && arr[j - 1].name > key.name) {
      arr[j] = arr[j - 1];
      --j;
    }
    arr[j] = key;
  }
  return arr;
}

template <std::size_t N>
[[nodiscard]] consteval std::array<std::string_view, kKindCount>
buildKindNames(const std::array<NamedKind, N> &arr) {
  std::array<std::string_view, kKindCount> names{};

  for (const auto &entry : arr) {
    if (entry.name.empty()) {
      throw "empty persona name";
    }

    const auto index = indexOf(entry.type);
    if (!names[index].empty()) {
      throw "duplicate persona enum";
    }

    names[index] = entry.name;
  }

  for (const auto &entry : names) {
    if (entry.empty()) {
      throw "missing persona name";
    }
  }

  return names;
}

template <std::size_t N>
consteval void validateUniqueNames(const std::array<NamedKind, N> &arr) {
  auto sorted = sortByName(arr);
  for (std::size_t i = 1; i < N; ++i) {
    if (sorted[i - 1].name == sorted[i].name) {
      throw "duplicate persona name";
    }
  }
}

inline constexpr auto kKindNames = buildKindNames(kNamedKinds);
inline constexpr auto kNamedKindsByName = sortByName(kNamedKinds);
inline constexpr bool kValidated = (validateUniqueNames(kNamedKinds), true);

inline constexpr std::array<std::string_view, 3> kTimingNames{{
    "consumer",
    "consumer_day",
    "business",
}};

} // namespace detail

[[nodiscard]] constexpr std::string_view name(Type type) noexcept {
  return detail::kKindNames[indexOf(type)];
}

[[nodiscard]] constexpr std::string_view toString(Type type) noexcept {
  return name(type);
}

[[nodiscard]] constexpr std::optional<Type> parse(std::string_view s) noexcept {
  std::size_t lo = 0;
  std::size_t hi = detail::kNamedKindsByName.size();

  while (lo < hi) {
    const std::size_t mid = lo + (hi - lo) / 2;
    const auto &entry = detail::kNamedKindsByName[mid];

    if (entry.name < s) {
      lo = mid + 1;
    } else if (entry.name > s) {
      hi = mid;
    } else {
      return entry.type;
    }
  }

  return std::nullopt;
}

[[nodiscard]] constexpr Type
parseOrDefault(std::string_view s, Type fallback = kDefaultType) noexcept {
  if (const auto parsed = parse(s)) {
    return *parsed;
  }
  return fallback;
}

[[nodiscard]] constexpr Type fromString(std::string_view s) noexcept {
  return parseOrDefault(s);
}

[[nodiscard]] constexpr std::string_view name(Timing timing) noexcept {
  return detail::kTimingNames[indexOf(timing)];
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

[[nodiscard]] constexpr Timing
parseTimingOrDefault(std::string_view s,
                     Timing fallback = Timing::consumer) noexcept {
  if (const auto parsed = parseTiming(s)) {
    return *parsed;
  }
  return fallback;
}

} // namespace PhantomLedger::personas
