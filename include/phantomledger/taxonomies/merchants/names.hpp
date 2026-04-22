#pragma once

#include "phantomledger/taxonomies/merchants/types.hpp"

#include <array>
#include <optional>
#include <string_view>

namespace PhantomLedger::taxonomies::merchants {
namespace detail {

struct NamedCategory {
  std::string_view name;
  Category category;
};

inline constexpr std::array<NamedCategory, kCategoryCount> kNamedCategories{{
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

template <std::size_t N>
[[nodiscard]] consteval std::array<NamedCategory, N>
sortByName(std::array<NamedCategory, N> arr) {
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
[[nodiscard]] consteval std::array<std::string_view, kCategoryCount>
buildNames(const std::array<NamedCategory, N> &arr) {
  std::array<std::string_view, kCategoryCount> names{};

  for (const auto &entry : arr) {
    if (entry.name.empty()) {
      throw "empty merchant category name";
    }

    const auto index = indexOf(entry.category);
    if (!names[index].empty()) {
      throw "duplicate merchant category enum";
    }

    names[index] = entry.name;
  }

  for (const auto &entry : names) {
    if (entry.empty()) {
      throw "missing merchant category name";
    }
  }

  return names;
}

template <std::size_t N>
consteval void validateUniqueNames(const std::array<NamedCategory, N> &arr) {
  auto sorted = sortByName(arr);
  for (std::size_t i = 1; i < N; ++i) {
    if (sorted[i - 1].name == sorted[i].name) {
      throw "duplicate merchant category name";
    }
  }
}

inline constexpr auto kCategoryNames = buildNames(kNamedCategories);
inline constexpr auto kNamedCategoriesByName = sortByName(kNamedCategories);
inline constexpr bool kValidated =
    (validateUniqueNames(kNamedCategories), true);

} // namespace detail

[[nodiscard]] constexpr std::string_view name(Category c) noexcept {
  return detail::kCategoryNames[indexOf(c)];
}

[[nodiscard]] constexpr std::string_view toString(Category c) noexcept {
  return name(c);
}

[[nodiscard]] constexpr std::optional<Category>
parse(std::string_view s) noexcept {
  std::size_t lo = 0;
  std::size_t hi = detail::kNamedCategoriesByName.size();

  while (lo < hi) {
    const std::size_t mid = lo + (hi - lo) / 2;
    const auto &entry = detail::kNamedCategoriesByName[mid];

    if (entry.name < s) {
      lo = mid + 1;
    } else if (entry.name > s) {
      hi = mid;
    } else {
      return entry.category;
    }
  }

  return std::nullopt;
}

inline constexpr std::array<Category, kCategoryCount> kDefaultCategories{
    Category::grocery,   Category::fuel,        Category::utilities,
    Category::telecom,   Category::ecommerce,   Category::restaurant,
    Category::pharmacy,  Category::retailOther, Category::insurance,
    Category::education,
};

} // namespace PhantomLedger::taxonomies::merchants
