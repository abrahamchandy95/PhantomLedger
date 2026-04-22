#pragma once

#include <cstddef>
#include <cstdint>

namespace PhantomLedger::taxonomies::merchants {

enum class Category : std::uint8_t {
  grocery,
  fuel,
  utilities,
  telecom,
  ecommerce,
  restaurant,
  pharmacy,
  retailOther,
  insurance,
  education,
};

inline constexpr std::size_t kCategoryCount =
    static_cast<std::size_t>(Category::education) + 1;

[[nodiscard]] constexpr std::size_t indexOf(Category c) noexcept {
  return static_cast<std::size_t>(c);
}

} // namespace PhantomLedger::taxonomies::merchants
