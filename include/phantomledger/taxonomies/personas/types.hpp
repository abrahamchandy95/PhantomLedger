#pragma once

#include <cstddef>
#include <cstdint>

namespace PhantomLedger::personas {

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

inline constexpr std::size_t kKindCount =
    static_cast<std::size_t>(Type::salaried) + 1;

inline constexpr Type kDefaultType = Type::salaried;

[[nodiscard]] constexpr std::size_t indexOf(Type type) noexcept {
  return static_cast<std::size_t>(type);
}

[[nodiscard]] constexpr std::size_t indexOf(Timing timing) noexcept {
  return static_cast<std::size_t>(timing);
}

} // namespace PhantomLedger::personas
