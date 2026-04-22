#pragma once

#include "phantomledger/taxonomies/personas/types.hpp"

#include <array>

namespace PhantomLedger::personas {
namespace detail {

inline constexpr std::array<bool, kKindCount> kHasEarnedIncome{{
    false, // student
    false, // retiree
    true,  // freelancer
    true,  // smallBusiness
    true,  // highNetWorth
    true,  // salaried
}};

} // namespace detail

[[nodiscard]] constexpr bool hasEarnedIncome(Type type) noexcept {
  return detail::kHasEarnedIncome[indexOf(type)];
}

// Compatibility alias if you want to keep older call sites unchanged.
[[nodiscard]] constexpr bool isEarner(Type type) noexcept {
  return hasEarnedIncome(type);
}

} // namespace PhantomLedger::personas
