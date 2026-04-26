#pragma once
/*
 * Identity primitives.
 * Key is the hashable ID type used across the registry; PersonId is
 * a strong typedef over u32 for a person index in the population
 * roster.
 */

#include "phantomledger/primitives/hashing/combine.hpp"
#include "phantomledger/taxonomies/identifiers/predicates.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace PhantomLedger::entity {

using Role = identifiers::Role;
using Bank = identifiers::Bank;

struct Key {
  Role role{};
  Bank bank = Bank::internal;
  std::uint64_t number = 0;

  constexpr std::strong_ordering operator<=>(const Key &) const = default;
};

[[nodiscard]] constexpr Key makeKey(Role role, Bank bank,
                                    std::uint64_t number) noexcept {
  return Key{role, bank, number};
}

[[nodiscard]] constexpr bool valid(const Key &k) noexcept {
  return k.number != 0 && identifiers::allows(k.role, k.bank);
}

Key makeKey(Role role, std::uint64_t number) = delete;

[[nodiscard]] inline std::size_t hashValue(const Key &k) noexcept {
  return hashing::make(static_cast<std::uint16_t>(k.role),
                       static_cast<std::uint8_t>(k.bank), k.number);
}

// --- PersonId ----------------------------------------------------

using PersonId = std::uint32_t;

inline constexpr PersonId invalidPerson = 0;

[[nodiscard]] constexpr bool valid(PersonId v) noexcept {
  return v != invalidPerson;
}

} // namespace PhantomLedger::entity

namespace std {

template <> struct hash<PhantomLedger::entity::Key> {
  std::size_t operator()(const PhantomLedger::entity::Key &k) const noexcept {
    return PhantomLedger::entity::hashValue(k);
  }
};

} // namespace std
