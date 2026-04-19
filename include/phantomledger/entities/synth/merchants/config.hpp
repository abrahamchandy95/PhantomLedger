#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace PhantomLedger::entities::synth::merchants {

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

[[nodiscard]] constexpr std::string_view toString(Category c) noexcept {
  switch (c) {
  case Category::grocery:
    return "grocery";
  case Category::fuel:
    return "fuel";
  case Category::utilities:
    return "utilities";
  case Category::telecom:
    return "telecom";
  case Category::ecommerce:
    return "ecommerce";
  case Category::restaurant:
    return "restaurant";
  case Category::pharmacy:
    return "pharmacy";
  case Category::retailOther:
    return "retail_other";
  case Category::insurance:
    return "insurance";
  case Category::education:
    return "education";
  }
  return "unknown";
}

inline constexpr std::array<Category, 10> defaultCategories{
    Category::grocery,   Category::fuel,        Category::utilities,
    Category::telecom,   Category::ecommerce,   Category::restaurant,
    Category::pharmacy,  Category::retailOther, Category::insurance,
    Category::education,
};

struct Config {
  struct Scale {
    double corePerTenK = 120.0;
    int coreFloor = 250;
    double sizeSigma = 1.2;
  } core;

  struct Tail {
    double perTenK = 400.0;
    double share = 0.18;
    double sizeSigma = 1.8;
  } tail;

  struct Banking {
    double internalP = 0.02;
  } banking;
};

} // namespace PhantomLedger::entities::synth::merchants
