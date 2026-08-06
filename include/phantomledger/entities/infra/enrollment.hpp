#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/devices.hpp"
#include "phantomledger/entities/infra/ipv4.hpp"
#include "phantomledger/primitives/hashing/constants.hpp"

#include <cstdint>

/*
  Which party -> endpoint associations the institution has on file.

  WHY COVERAGE IS PARTIAL RATHER THAN TOTAL. If every legitimate endpoint has
  an owning Party and no attacker endpoint does, "this endpoint has no Party
  edge" is a SYNONYM for "attacker endpoint" — precision 1.0 before a single
  transaction is read, and exporting the ownership topology hands a model the
  label. Production device data does not look like that:

    * A bank's device-binding registry is a SAMPLE of the endpoints its
      customers transact from. New handset, travel, a household laptop, a
      public network, an un-enrolled browser — legitimate rows arrive from
      endpoints with no recorded association all the time.
    * Plenty of fraud arrives from endpoints that ARE on file: trojans and
      remote-access scams drive the victim's own device, household fraud is
      committed on it, and residential proxies exit through some other
      customer's home address.

  So the registry's INCOMPLETENESS is modelled here, and the two populations
  are allowed to overlap in both directions (`injector.cpp` supplies the other
  direction via its victim-device and residential-proxy branches). "Endpoint
  not on file" is then what it is in production: a real, WEAK, useful feature
  worth a few multiples of base rate, not a lookup.
  `tests/test_card_endpoint_graph.cpp` sizes that lift and fails if it climbs
  back toward determinism.

  DRAW-FREE BY CONSTRUCTION — a stable hash of (person, endpoint), never a
  coin. A coin per usage would add draws inside the device/IP generators and
  shift EVERY downstream draw in the run, re-rolling the corpus for a fact
  that has no business perturbing amounts or timestamps. It is also a pure
  function of world state, so the value is identical in a full run and in any
  stream prefix — `Has_Device`/`Has_IP` are classified WORLD-DERIVED by
  `tests/test_card_point_in_time.cpp`, which demands full-vs-prefix byte
  identity, and a stream-observed or RNG-ordered decision could not satisfy
  that.

  CLASS S, CALIBRATION UNCITED. The levels are a declared modelling choice;
  what is defensible is the SHAPE — a registry mostly but materially
  incomplete, with device binding better covered than address association,
  since a device can be enrolled at authentication time while a residential IP
  is only ever observed. Promote to CITED and re-pin in whatever arc wires a
  named device-binding-coverage series.
 */

namespace PhantomLedger::infra::enrollment {

// Share of true (party, endpoint) associations the institution has on
// file. See the class-S note above.
inline constexpr double kDeviceCoverage = 0.72;
inline constexpr double kIpCoverage = 0.61;

namespace detail {

[[nodiscard]] constexpr std::uint64_t mix(std::uint64_t hash,
                                          std::uint64_t value) noexcept {
  for (int i = 0; i < 8; ++i) {
    hash ^= (value >> (8 * i)) & 0xFFU;
    hash *= ::PhantomLedger::hashing::constants::fnv64_prime;
  }
  return hash;
}

[[nodiscard]] constexpr std::uint64_t avalanche(std::uint64_t hash) noexcept {
  hash += 0x9E3779B97F4A7C15ULL;
  hash = (hash ^ (hash >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  hash = (hash ^ (hash >> 27U)) * 0x94D049BB133111EBULL;
  return hash ^ (hash >> 31U);
}

// Uniform in [0, 1) from the top 53 bits, the same construction
// `Pcg64::next_double` uses, so the threshold comparison below is the
// ordinary one.
[[nodiscard]] constexpr double unitOf(std::uint64_t hash) noexcept {
  return static_cast<double>(hash >> 11U) * 0x1.0p-53;
}

// A DOMAIN TAG PER ENDPOINT KIND. Without it a person's device draw and
// their IP draw would be correlated whenever the identity fields
// happened to collide, and "on file for the device implies on file for
// the address" is not a property the registry has.
inline constexpr std::uint64_t kDeviceDomain = 0xD3F1'CE00'0000'0001ULL;
inline constexpr std::uint64_t kIpDomain = 0x1'9A00'0000'0002ULL;

} // namespace detail

[[nodiscard]] constexpr bool
deviceEnrolled(entity::PersonId person, devices::Identity id,
               double coverage = kDeviceCoverage) noexcept {
  if (!(coverage > 0.0)) {
    return false;
  }
  if (coverage >= 1.0) {
    return true;
  }
  auto hash = ::PhantomLedger::hashing::constants::fnv64_offset;
  hash = detail::mix(hash, detail::kDeviceDomain);
  hash = detail::mix(hash, static_cast<std::uint64_t>(person));
  hash = detail::mix(hash, static_cast<std::uint8_t>(id.ownerType));
  hash = detail::mix(hash, id.ownerId);
  hash = detail::mix(hash, id.slot);
  return detail::unitOf(detail::avalanche(hash)) < coverage;
}

[[nodiscard]] constexpr bool
ipEnrolled(entity::PersonId person, network::Ipv4 address,
           double coverage = kIpCoverage) noexcept {
  if (!(coverage > 0.0)) {
    return false;
  }
  if (coverage >= 1.0) {
    return true;
  }
  auto hash = ::PhantomLedger::hashing::constants::fnv64_offset;
  hash = detail::mix(hash, detail::kIpDomain);
  hash = detail::mix(hash, static_cast<std::uint64_t>(person));
  hash = detail::mix(hash, address.value);
  return detail::unitOf(detail::avalanche(hash)) < coverage;
}

} // namespace PhantomLedger::infra::enrollment
