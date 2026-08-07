#pragma once

/*
  DRAW-FREE ENDPOINT DERIVATION — the endpoints that are computed rather than
  synthesized.

  Two populations are derived instead of minted, for two different reasons:

    * A PUBLIC TERMINAL'S ADDRESS. The terminal itself is world state
      (`infra::TerminalPool`, built on its own isolated lane); its exit address
      is not, because a terminal keeps ONE address for its whole service life
      and that address is a pure function of its identity. Deriving it puts a
      shared, high-fan-out address on the IP layer at zero world cost, which
      matters because the IP-side degree shortcut is measured at precision
      0.969-1.000 and closing the device axis alone would simply relocate it.
    * AN ATTACKER'S BURNER FINGERPRINT. A per-attempt device is by definition
      not persistent infrastructure, so minting it into the campaign pool would
      be a category error as well as a draw-count change.

  NOTHING HERE MAY EVER TAKE AN `Rng`, and both call sites depend on that:

    * `transactions::Factory::make` runs once per emitted row. A coin there
      would spend one uniform on EVERY routed row and re-roll every amount,
      timestamp and burst schedule in the corpus.
    * `buildCompromisePlans` pins its per-plan footprint at four uniforms,
      drawn unconditionally and in a fixed order, which is what confines
      endpoint work to the `device_id` / `ip_address` columns.

  The pattern is the one `commerce::affinity` established: a splitmix64
  finalizer over mixed words, one hash DOMAIN per predicate so that no two
  predicates keyed on the same plan sequence or victim share bits. Collinear
  domains would make one branch a deterministic function of another.

  NOTHING HERE READS THE FRAUD LABEL.
 */

/* `attackers.hpp` for `kAttackerOwnerIdBase` alone, which `isBurnerDevice`
 * needs. NOT a cycle: `attackers.hpp` includes only identifiers, devices,
 * ipv4 and tenure, and must stay that way. */
#include "phantomledger/entities/infra/attackers.hpp"
#include "phantomledger/entities/infra/devices.hpp"
#include "phantomledger/entities/infra/ipv4.hpp"
#include "phantomledger/primitives/hashing/constants.hpp"

#include <cstdint>

namespace PhantomLedger::infra::derived {

/* One domain per predicate, all distinct from `commerce::affinity`'s
 * `kAffinityDomain` and from `publicEndpoints::kHomeDomain` / `kUseDomain`,
 * for the reason `merchant_ownership.hpp` states: a shared domain makes two
 * supposedly independent draw-free decisions collinear on the same key. */
inline constexpr std::uint64_t kEndpointIpDomain = 0x4445'5256'0000'0001ULL;
inline constexpr std::uint64_t kBurnerSlotDomain = 0x4445'5256'0000'0002ULL;
inline constexpr std::uint64_t kBurnerCaseDomain = 0x4445'5256'0000'0003ULL;
inline constexpr std::uint64_t kBorrowCaseDomain = 0x4445'5256'0000'0004ULL;
/* 0x…0005 was the borrowed-HOST pick, retired when the host moved from a
 * uniform pick over the roster to `AttackerInfra::compromisedHosts`. The
 * value is left unassigned rather than reused: these are stable identities,
 * and re-issuing one would make a future predicate collinear with any
 * archived comparison against the old one. */
inline constexpr std::uint64_t kTerminalCaseDomain = 0x4445'5256'0000'0006ULL;
inline constexpr std::uint64_t kTerminalPickDomain = 0x4445'5256'0000'0007ULL;

[[nodiscard]] constexpr std::uint64_t splitmix(std::uint64_t value) noexcept {
  value += 0x9E37'79B9'7F4A'7C15ULL;
  value = (value ^ (value >> 30U)) * 0xBF58'476D'1CE4'E5B9ULL;
  value = (value ^ (value >> 27U)) * 0x94D0'49BB'1331'11EBULL;
  return value ^ (value >> 31U);
}

/* A stable uniform in [0,1) from an already-mixed word, taking the top 53
 * bits exactly as `affinity::unitFor` does. */
[[nodiscard]] constexpr double unitFrom(std::uint64_t mixed) noexcept {
  return static_cast<double>(splitmix(mixed) >> 11U) * 0x1.0p-53;
}

/* Mix a device identity. The owner TYPE is mixed in, so a terminal and a
 * campaign device carrying the same numeric owner id derive different
 * addresses. */
[[nodiscard]] constexpr std::uint64_t
mixIdentity(devices::Identity id) noexcept {
  const auto kind =
      static_cast<std::uint64_t>(static_cast<std::uint8_t>(id.ownerType));
  return splitmix(id.ownerId + hashing::constants::fnv64_prime) ^
         splitmix((kind << 40U) ^ static_cast<std::uint64_t>(id.slot));
}

/* THE SAME SUPPORT `network::randomIpv4` DRAWS FROM — first octet 11..222,
 * last octet 1..254 — and that is load-bearing, not cosmetic. A reserved or
 * fixed range for derived addresses would let an octet lookup separate a
 * terminal from a consumer line, and on the attacker side it would label
 * unauthorized fraud outright. Only the SOURCE of the entropy differs; the
 * distribution does not. */
[[nodiscard]] constexpr network::Ipv4 ipv4From(std::uint64_t seed) noexcept {
  const auto lo = splitmix(seed ^ kEndpointIpDomain);
  const auto hi = splitmix(lo);
  return network::Ipv4::pack(
      static_cast<std::uint8_t>(11U + (lo % 212U)),
      static_cast<std::uint8_t>((lo >> 24U) % 256U),
      static_cast<std::uint8_t>((hi >> 8U) % 256U),
      static_cast<std::uint8_t>(1U + ((hi >> 32U) % 254U)));
}

/* The address a derived endpoint exits through, stable for the life of the
 * endpoint. Applied to a public terminal it gives the IP layer the same
 * legitimate high-fan-out population the device layer gets; applied to a
 * burner it burns the address with the fingerprint. */
[[nodiscard]] constexpr network::Ipv4
endpointIp(devices::Identity id) noexcept {
  return ipv4From(mixIdentity(id));
}

/* Share of attacker-operated cases run on a FRESH fingerprint burned after
 * the case — the anti-detect mode — rather than on the campaign's persistent
 * device line.
 *
 * DIRECTION CITED: the card-not-present cash-out economy is priced PER
 * FINGERPRINT. Genesis Store sold stolen browser-fingerprint profiles at
 * $5-$200 each, held ~306,000 in stock and added ~18,000 per day;
 * "Antidetect" sold 40,000 unique fingerprints for $5,000 (Recorded Future /
 * Gemini Advisory, "Carding in the Time of COVID", 2020; origin documented in
 * Krebs on Security, March 2015; accessed 2026-08-07). A per-attempt
 * fingerprint is an ordinary operating mode, and the pre-existing model —
 * every case on a shared campaign device — represented only the high-fan-out
 * half of a bimodal population, which is precisely what let a degree
 * threshold score at ceiling. The LEVEL is a DECLARED CHOICE; no source gives
 * the mix.
 *
 * A burner is NOT in `AttackerInfra::sortedDevices`, so `ownsDevice` is false
 * for it. That is correct rather than incidental: a fingerprint bought for one
 * attempt is not persistent infrastructure, and it keeps burners out of the
 * reuse floors that measure whether the campaign mechanism still fires. */
inline constexpr double kBurnerEndpointShare = 0.15;

/* Share of attacker-operated cases run from an UNRELATED CUSTOMER'S DEVICE —
 * the device-side twin of the residential proxy: a consumer machine under
 * malware or in a botnet, still carrying its owner's ordinary traffic.
 *
 * THIS IS THE ONLY MECHANISM THAT PUTS FRAUD AGAINST ONE VICTIM ONTO A DEVICE
 * THAT ALSO CARRIES ANOTHER PERSON'S LEGITIMATE ROWS. Attacker devices are
 * otherwise pure-fraud by construction, and that purity — not the fan-out
 * level — is what lets a degree threshold reach precision 1.0000.
 *
 * CLASS S UNCITED. No named issuer-side series was found. Its sibling
 * `kResidentialProxyShare` is itself uncited and sits at 0.30; this ships
 * lower because a borrowed device is a heavier compromise than a borrowed
 * exit address. */
inline constexpr double kBorrowedDeviceShare = 0.25;

/* Share of attacker-operated cases run from a PUBLIC TERMINAL — carding from a
 * library, hotel business centre or internet cafe.
 *
 * THIS BRANCH EXISTS TO STOP THE ROUND FROM INVERTING THE DEFECT IT CLOSES.
 * The public-terminal and carrier-NAT pools are what give LEGITIMATE endpoints
 * degrees in the hundreds, which is what breaks "many cards on one device
 * implies fraud". But a high-degree population that carries no fraud AT ALL is
 * the original defect with the sign flipped: "degree above the consumer range
 * implies LEGITIMATE" becomes exactly as learnable as the rule it replaced,
 * and `attacker-infra-2026-07` rule 5 is explicit that closing one shortcut
 * must not open another. Measured before this branch existed, the top fan-out
 * buckets sat at 0.30-0.44x the base fraud rate with no mechanism able to put
 * a single fraud row on them.
 *
 * The host is picked the same way the borrowed branch picks its person, so
 * terminals are reached in proportion to their REACH — a busy terminal sees
 * more carding than a quiet one, which is the same law that governs its
 * legitimate traffic.
 *
 * CLASS S UNCITED. No source gives the share of card fraud operated from
 * public terminals. */
inline constexpr double kPublicTerminalShare = 0.12;

/* Burner slots live above every minted campaign slot. A campaign's own slots
 * are a small contiguous run from 0 — one per replacement link per line — so
 * 2^20 cannot collide with one. */
inline constexpr std::uint32_t kBurnerSlotBase = 1U << 20U;

/* A per-case fingerprint that keeps the campaign's owner type and owner id, so
 * internal attribution to the operator survives, and replaces only the slot.
 * The exporter hashes the complete triple into the one opaque `D...`
 * namespace, so nothing about the derivation reaches an exported id. */
[[nodiscard]] inline devices::Identity
burnerDeviceFrom(devices::Identity campaignDevice, std::uint64_t seq) noexcept {
  const auto mixed =
      splitmix(kBurnerSlotDomain ^ splitmix(seq) ^ mixIdentity(campaignDevice));
  return devices::Identity{
      campaignDevice.ownerType,
      campaignDevice.ownerId,
      kBurnerSlotBase + static_cast<std::uint32_t>(mixed % 0xF'FFFFU),
  };
}

/* IS THIS IDENTITY A BURNER? One definition, because there were three.
 *
 * The gate classifier, the exporter's ground-truth verdict and any future
 * reader all need the same structural test, and CLAUDE.md's standing
 * "prefer one constant over four copies" rule (the `kTableCount` lesson)
 * applies to predicates too. Pure, draw-free and total.
 *
 * THE TEST IS EXACT, NOT HEURISTIC. `burnerDeviceFrom` keeps the campaign's
 * owner type and owner id and puts the slot at or above `kBurnerSlotBase`;
 * a campaign's own slots are a small contiguous run from 0, and the only
 * other `OwnerType::ring` producer in the tree is the AML ring-shared map,
 * whose ids are small ring indices well below `kAttackerOwnerIdBase` and
 * whose slot is always 0. No legitimate row can reach this triple. */
[[nodiscard]] constexpr bool
isBurnerDevice(devices::Identity id) noexcept {
  return id.ownerType == devices::OwnerType::ring &&
         id.ownerId >= kAttackerOwnerIdBase && id.slot >= kBurnerSlotBase;
}

/* A stable uniform for a case-level branch. `seq` is the plan sequence and
 * `victim` the victim person id; both are fixed before the endpoint branch
 * runs, so this costs no draw and cannot move the planner's four-uniform
 * footprint. */
[[nodiscard]] inline double caseUnit(std::uint64_t domain, std::uint64_t seq,
                                     std::uint32_t victim) noexcept {
  return unitFrom(splitmix(domain ^ splitmix(seq)) ^
                  splitmix(static_cast<std::uint64_t>(victim) +
                           hashing::constants::fnv64_prime));
}

} // namespace PhantomLedger::infra::derived
