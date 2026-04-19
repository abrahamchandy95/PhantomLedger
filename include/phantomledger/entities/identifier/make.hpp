#pragma once

#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/entities/identifier/traits.hpp"

#include <cstdint>

namespace PhantomLedger::entities::identifier {

[[nodiscard]] constexpr Key make(Role role, Bank bank,
                                 std::uint64_t number) noexcept {
  return Key{role, bank, number};
}

[[nodiscard]] constexpr bool valid(const Key &value) noexcept {
  return value.number != 0 && allows(value.role, value.bank);
}

// Prevent the old implicit-bank style from creeping back in.
Key make(Role role, std::uint64_t number) = delete;

} // namespace PhantomLedger::entities::identifier
