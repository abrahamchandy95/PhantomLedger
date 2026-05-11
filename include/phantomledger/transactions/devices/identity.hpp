#pragma once
#include "phantomledger/primitives/hashing/combine.hpp"

#include <cstdint>
#include <functional>

namespace PhantomLedger::devices {

enum class OwnerType : std::uint8_t {
  person = 0,
  ring = 1,
  legitShared = 2,
  none = 3,
};

struct Identity {
  OwnerType ownerType = OwnerType::none;
  std::uint64_t ownerId = 0;
  std::uint32_t slot = 0;

  auto operator<=>(const Identity &) const = default;

  [[nodiscard]] static constexpr Identity none() noexcept { return Identity{}; }

  [[nodiscard]] static constexpr Identity person(std::uint64_t ownerId,
                                                 std::uint32_t slot) noexcept {
    return Identity{OwnerType::person, ownerId, slot};
  }

  [[nodiscard]] static constexpr Identity
  ring(std::uint64_t ringId, std::uint32_t slot = 0) noexcept {
    return Identity{OwnerType::ring, ringId, slot};
  }

  [[nodiscard]] constexpr bool assigned() const noexcept {
    return ownerType != OwnerType::none;
  }
};

[[nodiscard]] static constexpr Identity
legitShared(std::uint64_t groupId, std::uint32_t slot = 0) noexcept {
  return Identity{OwnerType::legitShared, groupId, slot};
}

[[nodiscard]] inline std::size_t hashValue(const Identity &value) noexcept {
  return PhantomLedger::hashing::make(
      static_cast<std::uint8_t>(value.ownerType), value.ownerId, value.slot);
}

} // namespace PhantomLedger::devices

namespace std {

template <> struct hash<PhantomLedger::devices::Identity> {
  std::size_t
  operator()(const PhantomLedger::devices::Identity &value) const noexcept {
    return PhantomLedger::devices::hashValue(value);
  }
};

} // namespace std
