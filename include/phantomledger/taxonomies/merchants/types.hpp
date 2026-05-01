#pragma once

#include <array>
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

// Ordered list of all categories — stable for iteration / sampling.
inline constexpr auto kCategories = std::to_array<Category>({
    Category::grocery,
    Category::fuel,
    Category::utilities,
    Category::telecom,
    Category::ecommerce,
    Category::restaurant,
    Category::pharmacy,
    Category::retailOther,
    Category::insurance,
    Category::education,
});

inline constexpr std::size_t kCategoryCount = kCategories.size();

} // namespace PhantomLedger::merchants
