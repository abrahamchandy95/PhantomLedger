#pragma once

/*
  WITHIN-CARDHOLDER MERCHANT AFFINITY — the Zipf visit law over a person's
  own favourite set.

  A uniform pick inside the favourite row is a Zipf exponent of ZERO, and
  that row carries ~99.2% of all merchant picks (`baseExploreP` is 0.02).
  Uniform is the wrong shape in both directions at once, and the two errors
  cancel in every total:

    * WITHIN a card it UNDER-concentrates — realized top-1 share of a
      cardholder's own visits is 1/F, i.e. 5.3% at the seeded F~19 and 2.6%
      at the F~38 the monthly evolver saturates to, against a measured 13%.
    * ACROSS cards it OVER-concentrates, because `Record.weight` then has no
      channel left but MEMBERSHIP: popularity enters as F independent
      inclusion Bernoullis, P(edge) = 1 - (1-w)^F. Measured at 8,000 people
      that put the top merchant on 51% of all cards while it carried ~2.2%
      of VOLUME against its own 2.56% nominal weight, and the concave map
      w -> (1-(1-w)^F)/F flattened the volume head at the same time.

  The merchant COUNT was never the defect: 250 core (`coreFloor`, which
  binds for every population below ~20,833) + 320 tail + 563 churn births =
  1,133 records, ~712 per 10,000 people, between the Census CBP
  employer-establishment density (248/10k, CBP 2022) and the Nilson
  card-accepting-location ceiling (~1,000/10k, YE2024).

  THE LAW. Merchant visitation is a Zipf rank-frequency law P(r) ~ r^-alpha
  — alpha = 0.80 for a North American issuer, 1.13 for a European one —
  which "holds independent of the total number of stores visited", so it
  rescales with the set size rather than pinning an absolute share. The top
  merchant takes ~13% of a North American cardholder's visits, ~22% of a
  European one's.

    CITED (accessed 2026-08-04): Krumme, Llorente, Cebrian, Pentland, Moro,
    "The predictability of consumer visitation patterns", Scientific Reports
    3:1645 (2013), Results + Figure 1. https://arxiv.org/abs/1305.1120

  THE ARITHMETIC IS SELF-CHECKING, which is what separates this from a
  plausible exponent: sum_{r=1}^{64} r^-0.80 = 7.48, so a deterministic rank
  law predicts a top-1 share of 1/7.48 = 13.4% at Krumme's own reported
  median set size of 64. That reproduces their separately-reported 13%
  without being fitted to it. `test_card_merchant_graph` sub-gate B
  EVALUATES that sum — a derivation in a comment is not a derivation until
  something evaluates it.

  THE RANK MUST STAY A HASH OF (person, merchant), NEVER THE SLOT INDEX.
  Three properties are load-bearing and the slot index has none of them:

  1. STABILITY UNDER `Csr::swapRemove`, which moves the last live entry over
     the hole, so slot positions are deliberately unordered. A slot-keyed
     weight would make a merchant's visit share jump whenever an unrelated
     favourite closed.
  2. DRAW-FREENESS. The pick must spend EXACTLY ONE uniform, so the
     favourite branch's draw count is unchanged; deriving the rank costs no
     draw.
  3. PREFIX-IDENTITY. A pure function of world state, so a short run is a
     byte-identical prefix of a longer one — the standing
     `test_card_point_in_time` requirement.

  The rank is `1 + floor(rowLength * u)` for a stable per-(person, merchant)
  uniform. Ranks COLLIDE, deliberately and measured: collisions inflate the
  normalising sum and push realized top-1 ~2pp BELOW the deterministic law
  (17.8% vs 21.7% at F=19, 12.7% vs 15.6% at F=48) — wrong in the safe
  direction for a use case whose failure mode is a hub. Realized top-1 lands
  at 12.7-17.8% across the operating range of F, inside Krumme's 13-22%
  NA-to-EU band.

  The rank scales with `rowLength` BY DESIGN, per Krumme's own
  independence-of-set-size finding: a person acquiring more favourites
  spreads their visits more thinly rather than holding an absolute share on
  the head. A merchant's share therefore jitters when the set changes, which
  happens at most monthly (the evolver's only hook) and is preference drift,
  not noise.

  DERIVE, DO NOT STORE. A cached parallel weight array would be
  O(favoriteCapacity) doubles per person — 96MB at the 500,000-person target
  on top of the 96MB the favourite CSR already costs — and would need
  invalidating from two places. The hash below is a splitmix64 finalizer
  over two mixed words: ~12 arithmetic operations, evaluated at most
  `favoriteCapacity` (48) times per merchant pick, no allocation and no
  invalidation surface. See docs/ram_derive_dont_store.md.
 */

#include "phantomledger/primitives/hashing/constants.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

namespace PhantomLedger::activity::spending::market::commerce {

/* The rank-frequency exponent of a cardholder's merchant visits. CITED —
 * Krumme et al. (2013), North American issuer. The European issuer measures
 * 1.13; NA is used because every other level here is US-anchored. */
inline constexpr double kVisitZipfAlpha = 0.80;

namespace affinity {

/* A distinct hash domain, so this shares no bits with any other draw-free
 * predicate keyed on the same person or merchant index (the two-domain
 * argument in `merchant_ownership.hpp` applies verbatim). */
inline constexpr std::uint64_t kAffinityDomain = 0x41'4646'4E00'0001ULL;

[[nodiscard]] constexpr std::uint64_t splitmix(std::uint64_t value) noexcept {
  value += 0x9E3779B97F4A7C15ULL;
  value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
  return value ^ (value >> 31U);
}

/* A stable uniform in [0,1) for the (person, merchant) pair. Both indices
 * are mixed before being combined, so (a,b) and (b,a) do not collide and
 * neither does an additive shift of one index. */
[[nodiscard]] constexpr double unitFor(std::uint32_t personIndex,
                                       std::uint32_t merchantIdx) noexcept {
  const auto mixed =
      splitmix(kAffinityDomain ^ splitmix(personIndex)) ^
      (splitmix(static_cast<std::uint64_t>(merchantIdx) +
                ::PhantomLedger::hashing::constants::fnv64_prime) *
       ::PhantomLedger::hashing::constants::golden64);
  return static_cast<double>(splitmix(mixed) >> 11U) * 0x1.0p-53;
}

/* This person's affinity weight for this merchant, given the size of their
 * favourite set. Proportional to rank^-alpha for a stable pseudo-rank in
 * [1, rowLength]. Never zero, so no favourite is unreachable. */
[[nodiscard]] inline double weightFor(std::uint32_t personIndex,
                                      std::uint32_t merchantIdx,
                                      std::size_t rowLength,
                                      double alpha = kVisitZipfAlpha) noexcept {
  if (rowLength <= 1) {
    return 1.0;
  }
  const double rank = 1.0 + std::floor(static_cast<double>(rowLength) *
                                       unitFor(personIndex, merchantIdx));
  return std::pow(rank, -alpha);
}

} // namespace affinity

/* Sample a SLOT of `row` by Zipf affinity, given `u` in [0,1).
 *
 * MUST SPEND NO DRAW — the caller supplies `u`, which is what keeps the
 * favourite branch at exactly one uniform.
 *
 * Returns 0 for an empty row; the caller already guards that (an empty
 * favourite row routes to the explore branch instead). */
[[nodiscard]] inline std::size_t
sampleFavoriteSlot(std::span<const std::uint32_t> row,
                   std::uint32_t personIndex, double u,
                   double alpha = kVisitZipfAlpha) noexcept {
  const auto length = row.size();
  if (length <= 1) {
    return 0;
  }

  double total = 0.0;
  for (const auto merchantIdx : row) {
    total += affinity::weightFor(personIndex, merchantIdx, length, alpha);
  }

  /* A non-finite or non-positive total cannot arise from rank^-alpha with
   * alpha > 0 and rank >= 1, but a caller-supplied alpha makes it
   * reachable; fall back to the head rather than reading past the row. */
  if (!(total > 0.0) || !std::isfinite(total)) {
    return 0;
  }

  double target = u * total;
  for (std::size_t slot = 0; slot + 1 < length; ++slot) {
    target -= affinity::weightFor(personIndex, row[slot], length, alpha);
    if (target < 0.0) {
      return slot;
    }
  }
  return length - 1;
}

} // namespace PhantomLedger::activity::spending::market::commerce
