#pragma once

#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <array>
#include <cstdint>

namespace PhantomLedger::personas {

namespace enumTax = ::PhantomLedger::taxonomies::enums;

namespace detail {

struct EarnedIncomeDefault {
  Type type;
  bool value = false;
};

inline constexpr auto kEarnedIncomeDefaults =
    std::to_array<EarnedIncomeDefault>({
        {Type::student, false},
        {Type::retiree, false},
        {Type::freelancer, true},
        {Type::smallBusiness, true},
        {Type::highNetWorth, true},
        {Type::salaried, true},
    });

static_assert(kEarnedIncomeDefaults.size() == kTypeCount);

[[nodiscard]] consteval bool coversEveryTypeOnce() {
  std::array<std::uint8_t, kTypeCount> seen{};

  for (const auto entry : kEarnedIncomeDefaults) {
    const auto index = enumTax::toIndex(entry.type);

    if (index >= kTypeCount) {
      return false;
    }

    ++seen[index];
  }

  for (const auto count : seen) {
    if (count != 1) {
      return false;
    }
  }

  return true;
}

[[nodiscard]] consteval std::array<bool, kTypeCount> earnedIncomeTable() {
  std::array<bool, kTypeCount> out{};

  for (const auto entry : kEarnedIncomeDefaults) {
    out[enumTax::toIndex(entry.type)] = entry.value;
  }

  return out;
}

static_assert(coversEveryTypeOnce());

inline constexpr auto kHasEarnedIncome = earnedIncomeTable();

} // namespace detail

[[nodiscard]] constexpr bool hasEarnedIncome(Type type) noexcept {
  return detail::kHasEarnedIncome[enumTax::toIndex(type)];
}

} // namespace PhantomLedger::personas
