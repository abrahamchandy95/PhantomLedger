#pragma once

#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/taxonomies/identifiers/predicates.hpp"

#include <cstdint>

namespace PhantomLedger::entities::identifier {

using taxonomies::identifiers::Bank;
using taxonomies::identifiers::Role;

[[nodiscard]] constexpr Key make(Role role, Bank bank,
                                 std::uint64_t number) noexcept {
  return Key{role, bank, number};
}

[[nodiscard]] constexpr bool valid(const Key &value) noexcept {
  return value.number != 0 &&
         taxonomies::identifiers::allows(value.role, value.bank);
}

Key make(taxonomies::identifiers::Role role, std::uint64_t number) = delete;

} // namespace PhantomLedger::entities::identifier
