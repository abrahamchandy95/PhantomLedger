#pragma once
#include "phantomledger/primitives/hashing/combine.hpp"

#include <cstdint>
#include <functional>

namespace PhantomLedger::devices {

/* APPEND ONLY. The numeric value is MIXED INTO THE EXPORTED IDENTIFIER —
 * `exporter::common::renderDeviceId` FNV-mixes
 * `static_cast<uint8_t>(ownerType)` before the splitmix avalanche — so
 * renumbering any existing member rewrites every device id of that kind across
 * `cf_Device`, `Transaction_Uses_Device`, `Has_Device` and the AML device
 * tables. `none` in particular must keep the value 3: it is the
 * default-constructed sentinel `assigned()` tests against, and
 * `binary_spool.cpp` round-trips the raw byte with no range check, so a spooled
 * 3 would silently decode as a different owner kind. */
enum class OwnerType : std::uint8_t {
  person = 0,
  ring = 1,
  legitShared = 2,
  none = 3,
  publicTerminal = 4,
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

  /* A shared/public terminal: a legitimate endpoint owned by NO person, held
   * by an operator rather than a customer. `terminalId` namespaces the
   * operator; `slot` discriminates its replacement chain, exactly as it does
   * for a personal line. */
  [[nodiscard]] static constexpr Identity
  publicTerminal(std::uint64_t terminalId, std::uint32_t slot = 0) noexcept {
    return Identity{OwnerType::publicTerminal, terminalId, slot};
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
