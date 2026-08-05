#pragma once
//
// phantomledger/entities/infra/enrollment.hpp
//
// WHICH PARTY -> ENDPOINT ASSOCIATIONS THE INSTITUTION HAS ON FILE.
//
// ===================================================================
// WHY A COVERAGE MODEL EXISTS AT ALL (attacker-infra-2026-07)
//
// `cf_Has_Device` / `cf_Has_IP` shipped HEADER-ONLY for four rounds, and
// the reasoning recorded against them was correct as far as it went:
// every legitimate endpoint had an owning Party and no attacker endpoint
// did, so "this endpoint has no Party edge" was a SYNONYM for "attacker
// endpoint" — precision 1.0, free of charge, before a single transaction
// was read. Exporting the ownership topology would have handed a model
// the label.
//
// But the reason it was a perfect rule is not that ownership is
// unknowable. It is that the generator made ownership TOTAL on one side
// and EMPTY on the other, with nothing in between. Production device
// data does not look like that:
//
//   * A bank's device-binding / trusted-endpoint registry is a SAMPLE of
//     the endpoints its customers actually transact from. New handset,
//     travel, a household laptop, a public network, an un-enrolled
//     browser — legitimate rows arrive from endpoints with no recorded
//     association all the time.
//   * Conversely, plenty of fraud arrives from endpoints that ARE on
//     file: banking trojans and remote-access scams drive the victim's
//     own device, household fraud is committed on it, and residential
//     proxy networks exit through some other customer's home address.
//
// So the honest fix is not to withhold the topology and not to invent
// ownership for exogenous infrastructure. It is to model the registry's
// INCOMPLETENESS, and to let the two populations overlap in both
// directions (see `injector.cpp` for the victim-device and
// residential-proxy mechanisms that supply the other direction).
// "Endpoint not on file" then becomes what it is in production: a real,
// WEAK, useful feature with a lift of a few multiples over base rate —
// not a lookup. `tests/test_card_endpoint_graph.cpp` SIZES that lift and
// fails if it ever climbs back toward determinism.
//
// ===================================================================
// DRAW-FREE BY CONSTRUCTION
//
// Coverage is a stable hash of (person, endpoint), not a coin. Two
// reasons, and the first is the load-bearing one:
//
//   * A coin per usage would add draws inside the device/IP generators
//     and shift EVERY downstream draw in the run — the corpus would
//     re-roll for a fact that has no business perturbing amounts or
//     timestamps. A hash costs nothing and moves nothing.
//   * It is a pure function of world state, so the value is identical in
//     a full run and in any stream prefix. `Has_Device`/`Has_IP` are
//     classified WORLD-DERIVED by `tests/test_card_point_in_time.cpp`,
//     which demands full-vs-prefix byte identity; a stream-observed or
//     RNG-ordered decision could not satisfy that.
//
// CLASS S, CALIBRATION UNCITED. The coverage levels below are a declared
// modelling choice. What is defensible is the SHAPE — a registry that is
// mostly complete but materially incomplete, device binding better
// covered than address association, since a device can be enrolled at
// authentication time while a residential IP is only ever observed.
// Promote to CITED and re-pin in whatever arc wires a named
// device-binding-coverage series.

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/devices.hpp"
#include "phantomledger/entities/infra/ipv4.hpp"
#include "phantomledger/primitives/hashing/constants.hpp"

#include <cstdint>

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
