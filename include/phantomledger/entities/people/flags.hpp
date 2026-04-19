#pragma once

#include <cstdint>

namespace PhantomLedger::entities::people {

enum class Flag : std::uint8_t {
  fraud = 1U << 0U,
  mule = 1U << 1U,
  victim = 1U << 2U,
  soloFraud = 1U << 3U,
};

[[nodiscard]] constexpr std::uint8_t bit(Flag flag) noexcept {
  return static_cast<std::uint8_t>(flag);
}

} // namespace PhantomLedger::entities::people
