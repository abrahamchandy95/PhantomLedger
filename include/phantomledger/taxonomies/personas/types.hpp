#pragma once

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

inline constexpr std::size_t kKindCount = 6;
inline constexpr Type kDefaultType = Type::salaried;

[[nodiscard]] constexpr std::size_t slot(Type t) noexcept {
  return static_cast<std::size_t>(t);
}

[[nodiscard]] constexpr std::size_t slot(Timing t) noexcept {
  return static_cast<std::size_t>(t);
}

} // namespace PhantomLedger::personas
