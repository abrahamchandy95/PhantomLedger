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

  CATEGORY DECIDES WHO HOLDS WHICH RANK (outlets-frequency-2026-09). The law
  above had no category input, so a grocery favourite and an insurer
  favourite drew from the same frequency distribution: favourite share
  equalled visit share to three decimals in every category, about 4.7
  payments per paying account a year whatever the merchant. The Diary of
  Consumer Payment Choice says otherwise by a factor of several (2022
  non-cash in-person payments a month: grocery 5.5, restaurants 5.4, general
  merchandise 3.1, gas 2.6).

  The fix keeps the row's rank MULTISET, {1 + floor(F * unitFor(p, m))}, and
  changes only its ASSIGNMENT. The row is ordered by a Plackett-Luce race key
  -ln(1 - v) / w_category (v a second hash, in its own domain), and the
  sorted rank uniforms go to merchants in race order, so a high-rate category
  tends to hold the head ranks. What that preserves, and why it is the
  construction rather than a multiplicative category factor:

  * The weights are the same numbers in a different order, so the
    normalising sum, the collision structure and the within-card top-1 share
    are unchanged per row, and Krumme's 0.80 law is untouched. A
    multiplicative factor w_c * rank^-0.80 was measured and rejected: solved
    to the same targets it pushes within-card top-1 from 0.150 to 0.206 at
    F = 30 and from 0.178 to 0.250 at F = 19, the second above Krumme's
    13-22% band (`test_card_merchant_graph` K7 prints the F = 30 figure).
  * Still a pure function of (person, set contents): swapRemove-invariant,
    draw-free, and a prefix of any longer run is identical.
  * The pick still spends exactly the one uniform the caller hands it.

  THE LIMIT, stated so nobody chases it: a category ranked LAST still takes
  the bottom ranks' Zipf mass, so reassignment alone cannot push the four
  biller categories below about 11.5% of card visits (measured at pop
  500,000, 2022, F = 30; the shipped table gives 12.1%, from 27.0% before).
  Going lower needs a membership change.
 */

#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/primitives/hashing/constants.hpp"
#include "phantomledger/taxonomies/merchants/types.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace PhantomLedger::activity::spending::market::commerce {

/* The rank-frequency exponent of a cardholder's merchant visits. CITED —
 * Krumme et al. (2013), North American issuer. The European issuer measures
 * 1.13; NA is used because every other level here is US-anchored. */
inline constexpr double kVisitZipfAlpha = 0.80;

/* Plackett-Luce race weight per category, in `merchants::Category` order.
 * DERIVED: solved so the card-present visit split lands on the Diary of
 * Consumer Payment Choice 2022 category rates with the online visit share
 * held on the dated CNP series; the arithmetic and the solve are the
 * outlets-frequency-2026-09 rows of docs/fraud_model_audit.md. Only the
 * RATIOS matter (grocery is 1). The 0.001 entries are a floor meaning
 * "ranked last": the solve drives pharmacy and the four biller categories to
 * zero, because even the bottom ranks give them more than their target. The
 * floor must sit well below the smallest solved weight: 1e-4 moves no
 * category share by more than 0.001, while 0.01 (the design's value) already
 * lifts the biller share from 0.121 to 0.131, since a biller then outraces an
 * ecommerce favourite about 6% of the time. */
using VisitRateTable =
    std::array<double, ::PhantomLedger::merchants::kCategoryCount>;
inline constexpr VisitRateTable kVisitRateWeight{
    1.0,   // grocery
    0.173, // fuel
    0.001, // utilities
    0.001, // telecom
    0.144, // ecommerce
    0.792, // restaurant
    0.001, // pharmacy
    0.307, // retailOther
    0.001, // insurance
    0.001, // education
};

/* What the race does to each category's share of card visits: visit share
 * over favourite share, per category, in `merchants::Category` order. The
 * category-blind law scores 1 everywhere. MEASURED, not chosen: sub-gate K of
 * `tests/test_card_merchant_graph.cpp` reads it at the solve point (pop
 * 500,000, 2022, F = 30) and asserts this table against the reading, so it
 * is re-read whenever `kVisitRateWeight` is re-solved. Pharmacy and the four
 * biller categories share one value because all five are ranked last: it is
 * the bottom ranks' Zipf mass over their count.
 *
 * Its consumer is the fraud venue pool (`fraud/typologies/unauthorized.cpp`),
 * which draws venues from the catalogue by weight, not from a favourite row.
 * Without this factor fraud card rows would keep the category-blind mix while
 * legitimate rows follow the Diary's, and merchant category would become a
 * fraud label (biller categories over-represented in fraud, grocery
 * under-represented). */
inline constexpr VisitRateTable kCategoryVisitLift{
    2.07, // grocery
    1.04, // fuel
    0.45, // utilities
    0.45, // telecom
    0.98, // ecommerce
    1.89, // restaurant
    0.45, // pharmacy
    1.30, // retailOther
    0.45, // insurance
    0.45, // education
};

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

/* The race hash's own domain, so the race order shares no bits with the
 * rank uniform above. */
inline constexpr std::uint64_t kRaceDomain = 0x41'4646'4E00'0002ULL;

[[nodiscard]] constexpr double raceUnit(std::uint32_t personIndex,
                                        std::uint32_t merchantIdx) noexcept {
  const auto mixed =
      splitmix(kRaceDomain ^ splitmix(personIndex)) ^
      (splitmix(static_cast<std::uint64_t>(merchantIdx) +
                ::PhantomLedger::hashing::constants::fnv64_prime) *
       ::PhantomLedger::hashing::constants::golden64);
  return static_cast<double>(splitmix(mixed) >> 11U) * 0x1.0p-53;
}

/* An Exp(rate) arrival: the smaller the key, the earlier in the race. */
[[nodiscard]] inline double
raceKey(std::uint32_t personIndex, std::uint32_t merchantIdx,
        ::PhantomLedger::merchants::Category category,
        const VisitRateTable &rates = kVisitRateWeight) noexcept {
  const double rate = rates[static_cast<std::size_t>(category)];
  return -std::log1p(-raceUnit(personIndex, merchantIdx)) / rate;
}

/* Fill `out[i]` with row[i]'s weight under the category-ranked law and return
 * their sum. `out` must hold `row.size()` values. A merchant index outside
 * the catalogue races at rate 1, which only a hand-built fixture can reach.
 *
 * Stack scratch covers every configured row (`favoriteCapacity` is 48); a
 * longer row takes the heap path and the same arithmetic. */
inline double rankedWeights(std::span<const std::uint32_t> row,
                            std::uint32_t personIndex,
                            const entity::merchant::Catalog &catalog,
                            const VisitRateTable &rates, double alpha,
                            std::span<double> out) {
  constexpr std::size_t kStack = 64;
  const auto n = row.size();
  if (n == 0) {
    return 0.0;
  }
  if (n == 1) {
    out[0] = 1.0;
    return 1.0;
  }

  std::array<double, kStack> uStack{};
  std::array<std::pair<double, std::uint32_t>, kStack> keyStack{};
  std::array<std::uint32_t, kStack> slotStack{};
  std::vector<double> uHeap;
  std::vector<std::pair<double, std::uint32_t>> keyHeap;
  std::vector<std::uint32_t> slotHeap;
  std::span<double> rankU{uStack.data(), n <= kStack ? n : 0};
  std::span<std::pair<double, std::uint32_t>> keys{keyStack.data(),
                                                   n <= kStack ? n : 0};
  std::span<std::uint32_t> slots{slotStack.data(), n <= kStack ? n : 0};
  if (n > kStack) {
    uHeap.resize(n);
    keyHeap.resize(n);
    slotHeap.resize(n);
    rankU = uHeap;
    keys = keyHeap;
    slots = slotHeap;
  }

  for (std::size_t i = 0; i < n; ++i) {
    const auto m = row[i];
    rankU[i] = unitFor(personIndex, m);
    const double key =
        m < catalog.records.size()
            ? raceKey(personIndex, m, catalog.records[m].category, rates)
            : -std::log1p(-raceUnit(personIndex, m));
    keys[i] = {key, m};
    slots[i] = static_cast<std::uint32_t>(i);
  }
  std::sort(rankU.begin(), rankU.end());
  // Ties on the key break on the merchant index, never the slot, so the order
  // is a property of the SET.
  std::sort(slots.begin(), slots.end(),
            [&](std::uint32_t a, std::uint32_t b) { return keys[a] < keys[b]; });

  double total = 0.0;
  for (std::size_t j = 0; j < n; ++j) {
    const double rank =
        1.0 + std::floor(static_cast<double>(n) * rankU[j]);
    const double w = std::pow(rank, -alpha);
    out[slots[j]] = w;
  }
  for (std::size_t i = 0; i < n; ++i) {
    total += out[i];
  }
  return total;
}

} // namespace affinity

/* Scan `weights` (aligned to the row) with `u`: the slot whose cumulative
 * mass first exceeds u * total. */
[[nodiscard]] inline std::size_t scanSlot(std::span<const double> weights,
                                          double total, double u) noexcept {
  const auto length = weights.size();
  if (!(total > 0.0) || !std::isfinite(total)) {
    return 0;
  }
  double target = u * total;
  for (std::size_t slot = 0; slot + 1 < length; ++slot) {
    target -= weights[slot];
    if (target < 0.0) {
      return slot;
    }
  }
  return length - 1;
}

/* THE PRODUCTION PICK: a SLOT of `row` under the category-ranked Zipf law,
 * given `u` in [0,1). Spends no draw. The legacy overload below (no
 * catalogue) keeps the category-blind law for the gate's comparison and its
 * disarm. */
[[nodiscard]] inline std::size_t
sampleFavoriteSlot(std::span<const std::uint32_t> row,
                   std::uint32_t personIndex, double u,
                   const entity::merchant::Catalog &catalog,
                   double alpha = kVisitZipfAlpha,
                   const VisitRateTable &rates = kVisitRateWeight) {
  const auto length = row.size();
  if (length <= 1) {
    return 0;
  }
  constexpr std::size_t kStack = 64;
  std::array<double, kStack> stack{};
  std::vector<double> heap;
  std::span<double> weights{stack.data(), length <= kStack ? length : 0};
  if (length > kStack) {
    heap.resize(length);
    weights = heap;
  }
  const double total = affinity::rankedWeights(row, personIndex, catalog,
                                               rates, alpha, weights);
  return scanSlot(weights, total, u);
}

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
