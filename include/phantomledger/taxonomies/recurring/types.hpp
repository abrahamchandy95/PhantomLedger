#pragma once

#include "phantomledger/taxonomies/enums.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace PhantomLedger::activity::recurring {

namespace enumTax = ::PhantomLedger::taxonomies::enums;

enum class Cadence : std::uint8_t {
  weekly = 0,
  biweekly = 1,
  semimonthly = 2,
  monthly = 3,
};

inline constexpr auto kPayCadences = std::to_array<Cadence>({
    Cadence::weekly,
    Cadence::biweekly,
    Cadence::semimonthly,
    Cadence::monthly,
});

inline constexpr std::size_t kPayCadenceCount = kPayCadences.size();

enum class WeekendRoll : std::uint8_t {
  previousBusinessDay = 0,
  nextBusinessDay = 1,
  none = 2,
};

inline constexpr auto kWeekendRolls = std::to_array<WeekendRoll>({
    WeekendRoll::previousBusinessDay,
    WeekendRoll::nextBusinessDay,
    WeekendRoll::none,
});

inline constexpr std::size_t kWeekendRollCount = kWeekendRolls.size();

static_assert(enumTax::isIndexable(kPayCadences));
static_assert(enumTax::isIndexable(kWeekendRolls));

} // namespace PhantomLedger::activity::recurring
