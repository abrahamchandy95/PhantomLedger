#pragma once

#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/taxonomies/merchants/types.hpp"

#include <array>
#include <cstdint>

namespace PhantomLedger::spending::config {

inline constexpr std::array<merchants::Category, 4> kBillerCategories{
    merchants::Category::utilities,
    merchants::Category::telecom,
    merchants::Category::insurance,
    merchants::Category::education,
};

[[nodiscard]] constexpr bool isBillerCategory(merchants::Category c) noexcept {
  for (const auto cat : kBillerCategories) {
    if (cat == c) {
      return true;
    }
  }
  return false;
}

struct MerchantPickRules {
  std::uint16_t maxPickAttempts = 250;
  std::uint16_t maxRetries = 6;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] {
      v::ge("maxPickAttempts", static_cast<int>(maxPickAttempts), 1);
    });
    r.check([&] { v::ge("maxRetries", static_cast<int>(maxRetries), 0); });
  }
};

inline constexpr MerchantPickRules kDefaultPickRules{};

} // namespace PhantomLedger::spending::config
