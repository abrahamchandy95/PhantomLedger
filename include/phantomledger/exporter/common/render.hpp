#pragma once

#include "phantomledger/encoding/layout.hpp"
#include "phantomledger/encoding/render.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/devices.hpp"
#include "phantomledger/primitives/hashing/constants.hpp"
#include "phantomledger/taxonomies/locale/names.hpp"

#include <cstddef>
#include <cstdint>

namespace PhantomLedger::exporter::common {

namespace entity = ::PhantomLedger::entity;
namespace encoding = ::PhantomLedger::encoding;
namespace devices = ::PhantomLedger::devices;
namespace locale = ::PhantomLedger::locale;

using CustomerId = encoding::RenderedId<24>;
using DeviceId = encoding::RenderedId<48>;

inline constexpr auto kUsCountry = locale::code(locale::Country::us);

[[nodiscard]] inline encoding::RenderedKey
renderKey(const entity::Key &k) noexcept {
  return encoding::format(k);
}

[[nodiscard]] inline CustomerId renderCustomerId(entity::PersonId person) {
  const auto key =
      entity::makeKey(entity::Role::customer, entity::Bank::internal, person);
  CustomerId out;
  out.length =
      static_cast<std::uint8_t>(encoding::write(out.bytes.data(), key));
  return out;
}

namespace detail {

// Stable FNV-1a field mixing, followed by a SplitMix64 avalanche. This is
// deliberately independent of std::hash so identifiers stay byte-identical
// across standard-library implementations. It is a pseudonym, not a security
// boundary; the objective is to keep generator roles out of the identifier.
[[nodiscard]] constexpr std::uint64_t
mixDeviceField(std::uint64_t hash, std::uint64_t value) noexcept {
  for (int i = 0; i < 8; ++i) {
    hash ^= (value >> (8 * i)) & 0xFFU;
    hash *= ::PhantomLedger::hashing::constants::fnv64_prime;
  }
  return hash;
}

[[nodiscard]] constexpr std::uint64_t
opaqueDeviceToken(devices::Identity id) noexcept {
  auto hash = ::PhantomLedger::hashing::constants::fnv64_offset;
  hash = mixDeviceField(hash, static_cast<std::uint8_t>(id.ownerType));
  hash = mixDeviceField(hash, id.ownerId);
  hash = mixDeviceField(hash, id.slot);

  hash += 0x9E3779B97F4A7C15ULL;
  hash = (hash ^ (hash >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  hash = (hash ^ (hash >> 27U)) * 0x94D049BB133111EBULL;
  return hash ^ (hash >> 31U);
}

} // namespace detail

[[nodiscard]] inline DeviceId renderDeviceId(devices::Identity id) {
  DeviceId out;
  if (!id.assigned()) {
    return out;
  }
  out.length = static_cast<std::uint8_t>(
      encoding::write(out.bytes.data(), encoding::kOpaqueDevice,
                      detail::opaqueDeviceToken(id)));
  return out;
}

} // namespace PhantomLedger::exporter::common
