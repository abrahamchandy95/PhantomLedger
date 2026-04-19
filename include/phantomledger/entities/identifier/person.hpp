#pragma once

#include <cstdint>

namespace PhantomLedger::entities::identifier {

using PersonId = std::uint32_t;

inline constexpr PersonId invalidPerson = 0;

[[nodiscard]] constexpr bool valid(PersonId value) noexcept {
  return value != invalidPerson;
}

} // namespace PhantomLedger::entities::identifier
