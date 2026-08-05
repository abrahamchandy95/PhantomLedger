#pragma once
//
// phantomledger/entities/counterparties/merchant_ownership.hpp
//
// WHICH MERCHANTS THE INSTITUTION HAS A BENEFICIAL OWNER ON FILE FOR.
//
// ===================================================================
// WHY THIS EXISTS (merchant-ownership-2026-07)
//
// `cf_Is_Merchant(merchant_id, party_id)` shipped HEADER-ONLY with the
// note "no modeled merchant-owning-party link (business owners own
// accounts, not catalog merchants)". That was an accurate reading of the
// world: `synth::accounts::assignBusinessOwners` mints `Role::business`
// keys owned by roster people, `synth::merchants::makeCatalog` mints
// `Role::merchant` keys with no owner at all, and the two populations are
// disjoint by construction — different Role, different serial space,
// never joined.
//
// The cost of leaving it empty was NOT confined to this repository.
// `tf_gnn_loader_v2` aborts the whole push at
// `sql/postgres/001_validate_sources.sql` with "cf_Is_Merchant is empty;
// Party_Is_Merchant edges would not load", so an empty table is not a
// quietly missing feature — it is a hard stop before any data reaches
// TigerGraph. The graph schema declares
// `Party_Is_Merchant(FROM Party, TO Merchant)` with
// `REVERSE_EDGE="Merchant_Owned_By_Party"`, and the loader's own
// `party_first_seen` derivation takes the MIN over a party's linked cards
// AND linked merchants, so the intended semantics is OWNERSHIP rather
// than any kind of merchant-category tagging.
//
// ===================================================================
// THE MODEL, AND WHAT IT IS NOT
//
// A share of catalog merchants are proprietor-run businesses whose owner
// is a customer of this institution, so the bank holds a beneficial-owner
// record for them. Owners are drawn from the EXISTING business-owner
// cohort — the roster people who already hold a `Role::business` account
// — because "the proprietor banks their business here" is what makes the
// bank able to know this at all. Coverage is PARTIAL by declaration: most
// of an acceptance catalog is other banks' merchants and corporately
// owned chains, for which no retail Party is the beneficial owner.
//
// IT IS NOT A CLAIM THAT THE MERCHANT ACQUIRES THROUGH THIS BANK.
// `Bank::internal` on a merchant key already means that, and it is 2% of
// core merchants — five merchants at population 10,000. Keying ownership
// on it would have produced a table with ~5 rows: technically non-empty,
// enough to satisfy the loader's abort, and carrying no graph structure
// whatsoever. That is the same "the count is not the measurement" trap
// the attacker-endpoint round was about, so it is refused here.
//
// ===================================================================
// THE PREDICATE IS A HASH OF THE MERCHANT KEY AND NOTHING ELSE
//
// This is a LEAK-CONTAINMENT decision and it is the reason the design
// looks blunter than it could.
//
// The tempting version keys eligibility on something meaningful — small
// outlets have proprietors, national services and online merchants do
// not. That would be more realistic AND it would be a shortcut: fraud
// destination selection is modality-conditioned (the card rail is ~70%
// card-not-present and draws ONLY from `Footprint::online`, while
// card-present draws only from physical outlets), so any eligibility rule
// that reads `footprint` or `weight` becomes correlated with the fraud
// label through the modality split. "Merchant has no owner edge" would
// then carry information about the ROW, which an ownership register has
// no business carrying.
//
// Hashing the key alone makes eligibility independent of size,
// footprint, geography, category and transaction history BY
// CONSTRUCTION, so the edge can only ever be graph STRUCTURE.
// `tests/test_card_endpoint_graph.cpp` measures the realized lift of
// "destination has an owner edge ⇒ fraud" and requires it to sit on 1.0.
//
// DRAW-FREE, for the same two reasons `infra::enrollment` is: it cannot
// perturb a single downstream draw (so the corpus stream does not move
// for a fact that has nothing to do with amounts or timestamps), and it
// is a pure function of world state, identical in a full run and in any
// stream prefix.
//
// CLASS S, CALIBRATION UNCITED. The coverage level is a declared
// modelling choice. What is defensible is that it is PARTIAL and that it
// is independent of every other merchant attribute.
//
// ===================================================================
// THE EDGE ASSERTS IDENTITY, NOT MONEY FLOW — BY DESIGN
// (OWNER RULING, 2026-07-28)
//
// This was briefly written up here as a "registered limitation". That was
// WRONG and the owner corrected it: an ownership edge is supposed to
// assert identity. `Party_Is_Merchant(FROM Party, TO Merchant)` carries
// `REVERSE_EDGE="Merchant_Owned_By_Party"` in the graph schema, and the
// only thing the loader derives from it is `party_first_seen` — a
// first-seen timestamp, not a balance. Nothing downstream asks it for a
// settlement path.
//
// So there is NO remittance leg to build, and adding one would be a
// clearing-layer change that moves every row count in the corpus to
// satisfy a claim this edge never makes. **A card purchase settles to the
// merchant's `Role::merchant` counterparty key, which is a sink. That is
// the model, not a gap in it.** The note survives only as guidance to a
// downstream analyst: use this edge for STRUCTURE (reaching a Party from a
// Merchant), never as evidence that funds moved.
//
// ===================================================================
// COVERAGE IS A DECLARED CHOICE, NOT AN OPEN QUESTION
//
// 45% reads as "45% of the merchants this bank's customers shop at are
// locally owned by its own business customers". At population 6,000 that
// means roughly half of the ~480 business-owner Parties are card-accepting
// merchants, which is plausible for a community-institution world; it
// would be too high for a nationwide acceptance footprint. Shipped at 0.45
// deliberately. It is the first number to revisit if this layer is ever
// calibrated against a source, and changing it is a one-constant,
// draw-free edit that moves `cf_Is_Merchant` alone.
//
// A fraud-actor Party CAN appear as a proprietor — the business-owner
// cohort spans the whole roster — and that is deliberate. Collusive
// merchants are real, `Party.is_fraud` is withheld as 0, and the actor
// label lives only in the quarantined overlay, so it is structure a model
// may legitimately learn rather than a label it can read.

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/hashing/constants.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

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

/// Is a beneficial owner on file for this merchant? Draw-free, and a
/// function of the merchant KEY alone — see the header note on why it
/// deliberately reads no other merchant attribute.
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

/// The proprietor Party for this merchant, or `invalidPerson` when no
/// owner is on file (or the cohort is empty). `owners` must be a
/// DETERMINISTICALLY ORDERED view of the business-owner cohort — the
/// caller sorts it, because the index this returns is positional and an
/// unordered cohort would make the mapping depend on container iteration
/// order rather than on world state.
[[nodiscard]] constexpr entity::PersonId
ownerFor(const entity::Key &merchantKey, std::span<const entity::PersonId> owners,
         double coverage = kBeneficialOwnerCoverage) noexcept {
  if (owners.empty() || !onFile(merchantKey, coverage)) {
    return entity::invalidPerson;
  }
  const auto pick = static_cast<std::size_t>(
      detail::keyHash(detail::kPickDomain, merchantKey) % owners.size());
  return owners[pick];
}

} // namespace PhantomLedger::entity::merchant::ownership
