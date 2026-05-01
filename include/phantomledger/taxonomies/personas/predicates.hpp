#pragma once

#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <array>

namespace PhantomLedger::personas {

using namespace ::PhantomLedger::taxonomies::enums;

namespace detail {

inline constexpr auto kHasEarnedIncome = std::to_array<bool>({
    false, // student
    false, // retiree
    true,  // freelancer
    true,  // smallBusiness
    true,  // highNetWorth
    true,  // salaried
});

static_assert(kHasEarnedIncome.size() == kTypeCount);

} // namespace detail

// --- Predicates --------------------------------------------------

[[nodiscard]] constexpr bool hasEarnedIncome(Type type) noexcept {
  return detail::kHasEarnedIncome[toIndex(type)];
}

} // namespace PhantomLedger::personas
