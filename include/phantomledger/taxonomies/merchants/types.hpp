#pragma once

#include <cstddef>
#include <cstdint>

namespace PhantomLedger::merchants {

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

inline constexpr std::size_t kCategoryCount = 10;

[[nodiscard]] constexpr std::size_t slot(Category c) noexcept {
  return static_cast<std::size_t>(c);
}

} // namespace PhantomLedger::merchants
