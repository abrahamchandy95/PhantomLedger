#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace PhantomLedger::personas {

// --- Types -------------------------------------------------------

enum class Type : std::uint8_t {
  student = 0,
  retiree = 1,
  freelancer = 2,
  smallBusiness = 3,
  highNetWorth = 4,
  salaried = 5,
};

enum class Timing : std::uint8_t {
  consumer = 0,
  consumerDay = 1,
  business = 2,
};

inline constexpr auto kTypes = std::to_array<Type>({
    Type::student,
    Type::retiree,
    Type::freelancer,
    Type::smallBusiness,
    Type::highNetWorth,
    Type::salaried,
});

inline constexpr auto kTimings = std::to_array<Timing>({
    Timing::consumer,
    Timing::consumerDay,
    Timing::business,
});

inline constexpr std::size_t kTypeCount = kTypes.size();
inline constexpr std::size_t kTimingCount = kTimings.size();

// Compatibility alias if other files still use kKindCount.
inline constexpr std::size_t kKindCount = kTypeCount;

inline constexpr Type kDefaultType = Type::salaried;

} // namespace PhantomLedger::personas
