#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/hashing/constants.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

/*
  Which merchants the institution has a beneficial owner on file for.

  A share of catalog merchants are proprietor-run businesses whose owner banks
  here, so the institution holds a beneficial-owner record. Owners come from
  the EXISTING business-owner cohort — roster people already holding a
  `Role::business` account — because "the proprietor banks their business
  here" is what makes the bank able to know this at all. Coverage is PARTIAL
  by declaration: most of an acceptance catalog is other banks' merchants and
  corporately owned chains, for which no retail Party is the owner.

  Populating it is not optional downstream: `tf_gnn_loader_v2` aborts the whole
  push at `sql/postgres/001_validate_sources.sql` when `cf_Is_Merchant` is
  empty, so an empty table is a hard stop, not a quietly missing feature.

  DO NOT KEY ELIGIBILITY ON `Bank::internal`. It means "acquires through this
  bank" and covers ~2% of core merchants — five merchants at population
  10,000. It would satisfy the loader's abort with a ~5-row table carrying no
  graph structure: a count, not a measurement.

  THE PREDICATE IS A HASH OF THE MERCHANT KEY AND NOTHING ELSE, and that is
  leak containment rather than bluntness. Keying eligibility on something
  meaningful — small outlets have proprietors, national and online merchants
  do not — would be more realistic AND a shortcut: fraud destination selection
  is modality-conditioned (the card rail is ~70% card-not-present and draws
  ONLY from `Footprint::online`; card-present draws only from physical
  outlets), so any rule reading `footprint` or `weight` correlates with the
  fraud label through the modality split, and "merchant has no owner edge"
  starts carrying information about the ROW. Hashing the key alone makes
  eligibility independent of size, footprint, geography, category and
  transaction history BY CONSTRUCTION, so the edge can only ever be graph
  structure. `tests/test_card_endpoint_graph.cpp` measures the realized lift
  of "destination has an owner edge => fraud" and requires it to sit on 1.0.

  DRAW-FREE, for the two reasons `infra::enrollment` is: it cannot perturb a
  downstream draw, and it is a pure function of world state, so the value is
  identical in a full run and in any stream prefix.

  THE EDGE ASSERTS IDENTITY, NOT MONEY FLOW. `Party_Is_Merchant(FROM Party, TO
  Merchant)` carries `REVERSE_EDGE="Merchant_Owned_By_Party"`, and the only
  thing the loader derives from it is `party_first_seen` — a timestamp, not a
  balance. DO NOT ADD A MERCHANT REMITTANCE LEG to "complete" it: a card
  purchase settles to the merchant's `Role::merchant` key, which is a sink,
  and adding a leg would be a clearing-layer change moving every row count in
  the corpus to satisfy a claim this edge never makes. Downstream, use it for
  STRUCTURE (reaching a Party from a Merchant), never as evidence funds moved.

  CLASS S, CALIBRATION UNCITED. 0.45 reads as "45% of the merchants this
  bank's customers shop at are locally owned by its own business customers" —
  at population 6,000 roughly half of the ~480 business-owner Parties, which
  is plausible for a community institution and too high for a nationwide
  acceptance footprint. What is defensible is that coverage is PARTIAL and
  independent of every other merchant attribute. Changing it is a
  one-constant, draw-free edit that moves `cf_Is_Merchant` alone.

  A fraud-actor Party CAN appear as a proprietor, deliberately: the
  business-owner cohort spans the whole roster, collusive merchants are real,
  `Party.is_fraud` is withheld as 0, and the actor label lives only in the
  quarantined overlay — so it is structure a model may legitimately learn
  rather than a label it can read.
 */

namespace PhantomLedger::entity::merchant::ownership {

// Share of catalog merchants for which a beneficial owner is on file.
inline constexpr double kBeneficialOwnerCoverage = 0.45;

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

// Two INDEPENDENT domains off the same key: one decides whether an owner
// is on file, the other decides which owner. Sharing one hash would tie
// "has an owner" to "which owner", which would make the owner index
// non-uniform over the cohort.
inline constexpr std::uint64_t kCoverageDomain = 0x4D'4552'4348'0001ULL;
inline constexpr std::uint64_t kPickDomain = 0x4D'4552'4348'0002ULL;

[[nodiscard]] constexpr std::uint64_t keyHash(std::uint64_t domain,
                                              const entity::Key &key) noexcept {
  auto hash = ::PhantomLedger::hashing::constants::fnv64_offset;
  hash = mix(hash, domain);
  hash = mix(hash, static_cast<std::uint8_t>(key.role));
  hash = mix(hash, static_cast<std::uint8_t>(key.bank));
  hash = mix(hash, key.number);
  return avalanche(hash);
}

[[nodiscard]] constexpr double unitOf(std::uint64_t hash) noexcept {
  return static_cast<double>(hash >> 11U) * 0x1.0p-53;
}

} // namespace detail

/* Is a beneficial owner on file for this merchant? Draw-free, and a
 * function of the merchant KEY alone — see the header note on why it
 * deliberately reads no other merchant attribute. */
[[nodiscard]] constexpr bool
onFile(const entity::Key &merchantKey,
       double coverage = kBeneficialOwnerCoverage) noexcept {
  if (!(coverage > 0.0)) {
    return false;
  }
  if (coverage >= 1.0) {
    return true;
  }
  return detail::unitOf(detail::keyHash(detail::kCoverageDomain, merchantKey)) <
         coverage;
}

/* The proprietor Party for this merchant, or `invalidPerson` when no
 * owner is on file (or the cohort is empty). `owners` must be a
 * DETERMINISTICALLY ORDERED view of the business-owner cohort — the
 * caller sorts it, because the index this returns is positional and an
 * unordered cohort would make the mapping depend on container iteration
 * order rather than on world state. */
[[nodiscard]] constexpr entity::PersonId
ownerFor(const entity::Key &merchantKey,
         std::span<const entity::PersonId> owners,
         double coverage = kBeneficialOwnerCoverage) noexcept {
  if (owners.empty() || !onFile(merchantKey, coverage)) {
    return entity::invalidPerson;
  }
  const auto pick = static_cast<std::size_t>(
      detail::keyHash(detail::kPickDomain, merchantKey) % owners.size());
  return owners[pick];
}

} // namespace PhantomLedger::entity::merchant::ownership
