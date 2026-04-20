#pragma once

#include <cstddef>
#include <cstdint>

namespace PhantomLedger::entities::landlords {

enum class Class : std::uint8_t {
  individual = 0,
  llcSmall = 1,
  corporate = 2,
};

inline constexpr std::size_t kClassCount = 3;

[[nodiscard]] constexpr std::size_t
classIndex(entities::landlords::Class kind) noexcept {
  return static_cast<std::size_t>(kind);
}
} // namespace PhantomLedger::entities::landlords
