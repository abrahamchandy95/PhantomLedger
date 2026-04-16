#pragma once
/*
 * people.hpp — builders for person-side entity identities.
 */

#include "phantomledger/entities/identity.hpp"

#include <cstdint>

namespace PhantomLedger::entities::people {

[[nodiscard]] constexpr Identity customer(std::uint64_t number) noexcept {
  return Identity{Type::customer, BankAccount::internal, number};
}

} // namespace PhantomLedger::entities::people
