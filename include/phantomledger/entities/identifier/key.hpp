#pragma once

#include "phantomledger/primitives/hashing/combine.hpp"
#include "phantomledger/taxonomies/identifiers/types.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace PhantomLedger::entities::identifier {

using taxonomies::identifiers::Bank;
using taxonomies::identifiers::Role;

struct Key {
  Role role{};
  Bank bank = Bank::internal;
  std::uint64_t number = 0;

  std::strong_ordering operator<=>(const Key &) const = default;
};

[[nodiscard]] inline std::size_t hashValue(const Key &value) noexcept {
  return PhantomLedger::hashing::make(static_cast<std::uint16_t>(value.role),
                                      static_cast<std::uint8_t>(value.bank),
                                      value.number);
}

} // namespace PhantomLedger::entities::identifier

namespace std {

template <> struct hash<PhantomLedger::entities::identifier::Key> {
  std::size_t operator()(
      const PhantomLedger::entities::identifier::Key &value) const noexcept {
    return PhantomLedger::entities::identifier::hashValue(value);
  }
};

} // namespace std
