#pragma once
/*
 * identity.hpp — compact identity for graph entities.
 *
 * Identity carries the three fields needed to distinguish an entity in
 * hot-path code:
 *   - entity type
 *   - on-us/off-us banking relationship
 *   - numeric sequence
 */

#include "phantomledger/entities/categories.hpp"
#include "phantomledger/hashing/combine.hpp"

#include <cstdint>
#include <functional>

namespace PhantomLedger::entities {

struct Identity {
  Type type{};
  BankAccount bank = BankAccount::internal;
  std::uint64_t number = 0;

  auto operator<=>(const Identity &) const = default;
};

[[nodiscard]] inline std::size_t hashValue(const Identity &value) noexcept {
  return PhantomLedger::hashing::make(static_cast<std::uint16_t>(value.type),
                                      static_cast<std::uint8_t>(value.bank),
                                      value.number);
}

} // namespace PhantomLedger::entities

namespace std {

template <> struct hash<PhantomLedger::entities::Identity> {
  std::size_t
  operator()(const PhantomLedger::entities::Identity &value) const noexcept {
    return PhantomLedger::entities::hashValue(value);
  }
};

} // namespace std
