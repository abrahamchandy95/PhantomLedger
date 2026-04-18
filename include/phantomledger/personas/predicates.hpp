#pragma once

#include "phantomledger/personas/taxonomy.hpp"

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

[[nodiscard]] constexpr bool hasEarnedIncome(Kind kind) noexcept {
  return detail::kHasEarnedIncome[indexOf(kind)];
}

// Compatibility alias if you want to keep older call sites unchanged.
[[nodiscard]] constexpr bool isEarner(Kind kind) noexcept {
  return hasEarnedIncome(kind);
}

} // namespace PhantomLedger::personas
