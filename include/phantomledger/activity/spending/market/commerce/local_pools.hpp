#pragma once
//
// phantomledger/activity/spending/market/commerce/local_pools.hpp
//
// FACTORISED DISTANCE-DECAY POOLS (merchant-selection-2026-08 step 2).
//
// ===================================================================
// THE REALISM DEFECT THIS CLOSES
//
// Favourite-set MEMBERSHIP was drawn from a GEOGRAPHY-FREE national CDF, and
// the favourite row carries ~99.2% of all merchant picks. So the mean
// home-to-favourite distance was **1,206 miles** with only **4.96%** of
// favourites inside 50 miles of home, and the single most-reached merchant —
// a PHYSICAL outlet with one centroid — was held by cardholders living in
// **all 71 of the catalogue's 71 US home areas**.
//
// `merchant-selection-2026-08` step 1 capped how MANY cards reach one
// merchant (0.51 -> 0.12) by flattening the membership law, but flattening is
// geography-blind: it made the Phoenix corner store less likely to be
// favourited by ANYONE without making its holders LOCAL. A single acceptance
// location serving a nationwide cardholder base is impossible, and
// `entities/counterparties/merchants.hpp` commits to outlet semantics.
//
// THE LEAK THAT MADE IT URGENT. The fraud card-present rail IS distance-decayed
// (`unauthorized.cpp` applies `exp(-miles/scaleMiles)`), landing 0-11 miles
// from home, while legitimate favourites landed 1,206 miles away. So
// `distance(cardholder_home, merchant)` separated the labels at ~7x lift on a
// 0.31% base rate — a shortcut a GNN finds instead of graph structure, and
// INVERTED versus reality, where card-present fraud is displaced AWAY from the
// victim. `attacker-infra` rule 5: closing one shortcut must not open another.
//
// ===================================================================
// THE MEMORY DEFECT, AND WHY ONE STRUCTURE FIXES BOTH
//
// `GeographicMerchantPools` holds a DENSE `occupiedAreas x physicalMerchants`
// double matrix, TWICE (cached base weights plus the live CDF). Measured 28 MB
// at 500,000 people over 2 years, 51 MB over 20 years, and it is O(A*M) — so
// 500 areas would need 197 MB and US-county granularity 1.24 GB, which is a
// hard blocker on ever extending `geo_data.hpp`.
//
// **That matrix stores the same vector 71 times.** Coordinates are AREA
// CENTROIDS (`merchant-coordinates` rule 1), so every merchant in area `a` is
// at the SAME distance from any home `h`. The weight therefore FACTORISES:
//
//     weight(h, m) = w_m * exp(-dist(h, area(m)) / scale(h))
//                  = w_m * decay(h, area(m))
//
// and for all m in one area the decay term is a constant, so
//
//     P(m | h, area(m) == a) = w_m / sum_{m' in a} w_{m'}     <- NO h
//
// The within-area distribution is HOME-INDEPENDENT. Sampling becomes:
//   1. pick an area from a CDF over `decay(h,a) * totalWeight(a)`  (<= 16 entries)
//   2. pick a merchant from that area's own CDF                    (stored ONCE)
//
// Memory drops from O(A*M) to O(M + A*k): measured **0.35 MB versus 28 MB at
// the 500,000-person target, an ~80x reduction** — while ADDING a second
// weight law (one instance for volume-driven exploration, one for
// reach-driven membership). The monthly `rebuildLive` cost falls the same way.
//
// EXACT, not approximate, for the within-area half — it is an algebraic
// refactor of the same product. The only approximation is the inter-area
// CUTOFF below.
//
// ===================================================================
// ONE UNIFORM, BY SUCCESSIVE RESCALING
//
// Two-stage sampling would normally cost two draws, which would change the
// hot path's draw count and break `WeightedPicker`'s accounting. Instead the
// single `u` is consumed twice: after locating the area's cumulative bracket
// `[lo, hi)`, the residual `(u - lo) / (hi - lo)` is again uniform on [0,1)
// and independent of the bracket, so it drives the within-area pick. Exact,
// and the draw count stays at exactly ONE per attempt.
//
// ===================================================================
// THE INTER-AREA CUTOFF IS ON GEOMETRY ALONE, AND THAT IS DELIBERATE
//
// Areas are kept per home in descending `decay` order until the decay falls
// below `kDecayFloorRatio` of the home's best, capped at `kMaxNearbyAreas`,
// minimum ONE. Sparsifying on decay (pure geometry, constant for the run)
// rather than on `decay * totalWeight` is what keeps the retained SET stable
// across the monthly liveness rebuild — a set that changed with liveness would
// make the sampled distribution depend on rebuild history, which is exactly
// the statefulness `attacker-infra` rule 3 forbids between the batch and
// windowed engines.
//
// The floor is safe because decay spans many orders of magnitude: at the
// 4-mile dense-urban scale the 8th-nearest area already contributes ~1e-11.
// The minimum of one is what keeps a home area with NO nearby merchants
// (Burlington VT at pop 8,000 has zero physical outlets of its own) sampling
// from its nearest neighbours instead of collapsing to an empty pool.
// `retainedMassFloor()` reports the worst-case retained mass so a gate can
// assert the cutoff is not silently discarding reachable merchants.
//

#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/activity/spending/market/commerce/reach.hpp"
#include "phantomledger/primitives/random/distributions/cdf.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::activity::spending::market::commerce {

// `buildCdf` THROWS on an empty span, a negative weight, or a non-positive
// sum — it never returns an empty vector. Liveness masking can legitimately
// produce an all-zero weight vector (every merchant in one local pool closed in
// the same month), which is a fallback condition rather than a bug, so it needs
// a total-first check instead of a try/catch or a post-hoc `.empty()` test. The
// post-hoc test was the first attempt and it was DEAD CODE: the exception fires
// before the caller can inspect the result.
//
// Moved here from the retired `geo_pools.hpp` unchanged. It is a CDF utility
// rather than a geography concept, and it outlived the dense pool class it
// happened to share a file with.
[[nodiscard]] inline std::vector<double>
cdfOrEmpty(const std::vector<double> &weights) {
  if (weights.empty()) {
    return {};
  }
  double total = 0.0;
  for (const double w : weights) {
    if (!(w >= 0.0)) {
      return {};
    }
    total += w;
  }
  if (!(total > 0.0) || !std::isfinite(total)) {
    return {};
  }
  return probability::distributions::buildCdf(weights);
}


// THE CARD-NOT-PRESENT SHARE OF A CARDHOLDER'S MERCHANT SET, BY YEAR.
//
// The share of favourite draws that route to the geography-free ONLINE branch.
// It used to be DERIVED from the catalogue's online reach mass, which came out
// at 0.118-0.138 — and that was wrong twice over.
//
// WRONG ON LEVEL. The Federal Reserve Payments Study reports, from the NPIPS
// census of the major card networks: **"In 2022, in-person payments were 63.8
// percent of total GP card payments by number"** — so remote (card-not-present)
// is **36.2% by number**, roughly 2.8x the share the catalogue mass implied.
//   CITED (accessed 2026-08-05): Federal Reserve Payments Study, National
//   Payment Volumes Detailed Data (CY 2021 and 2022).
//   https://www.federalreserve.gov/paymentsystems/2024-November-The-Federal-Reserve-Payments-Study.htm
//
// Worth stating plainly because the intuition runs the other way: in-person is
// still the MAJORITY of card transactions by count, nearly 2:1. E-commerce is
// only 16.9% of total retail SALES (Census, Q1 2026). CNP exceeds the
// e-commerce share because it also carries phone and mail order, recurring
// subscription billing, and in-app payments.
//
// WRONG ON TIME, and that is the bigger error over a multi-decade window. A
// flat share is the `burst-rate-2026-07` mistake in another costume: card
// e-commerce barely existed in 1991, so a 20-year run with a constant 36.2%
// misstates both ends. The shape below is the Census e-commerce share of total
// retail sales — a published quarterly series back to 1999 — scaled by 2.46 so
// 2022 lands exactly on the Fed's cited 36.2%.
//   SHAPE CITED: US Census Bureau, Quarterly Retail E-Commerce Sales.
//   LEVEL CITED: Fed Payments Study 2022, above.
//   THE 2.46x MULTIPLIER IS A DECLARED CHOICE (CLASS S) — it is the ratio of
//   CNP-by-number to e-commerce-share-of-retail-sales at the one year where
//   both are published, and it is held constant across the window.
//
// Pre-1999 is extrapolated toward mail/phone order only and is CLASS S UNCITED.
struct CnpSharePoint {
  int year;
  double share;
};
inline constexpr std::array<CnpSharePoint, 9> kCnpShareByYear{{
    {1991, 0.010}, // mail/phone order only — UNCITED extrapolation
    {1995, 0.014},
    {1999, 0.015}, // Census e-comm 0.6% of retail x 2.46
    {2005, 0.059}, // 2.4%
    {2010, 0.108}, // 4.4%
    {2015, 0.180}, // 7.3%
    {2019, 0.271}, // 11.0%
    {2022, 0.362}, // 14.7% -> the CITED Fed anchor, by construction
    {2026, 0.416}, // 16.9% (Census Q1 2026)
}};

// Linear interpolation between the pinned points, clamped outside coverage —
// the same freeze-and-declare discipline `macro-history-v1` uses for the
// nominal scales.
[[nodiscard]] inline double cnpShareForYear(int year) noexcept {
  if (year <= kCnpShareByYear.front().year) {
    return kCnpShareByYear.front().share;
  }
  if (year >= kCnpShareByYear.back().year) {
    return kCnpShareByYear.back().share;
  }
  for (std::size_t i = 1; i < kCnpShareByYear.size(); ++i) {
    if (year <= kCnpShareByYear[i].year) {
      const auto &lo = kCnpShareByYear[i - 1];
      const auto &hi = kCnpShareByYear[i];
      const double t = static_cast<double>(year - lo.year) /
                       static_cast<double>(hi.year - lo.year);
      return lo.share + t * (hi.share - lo.share);
    }
  }
  return kCnpShareByYear.back().share;
}

// Retain nearby areas whose decay is at least this fraction of the home's
// best.
inline constexpr double kDecayFloorRatio = 1e-9;

// Hard cap on retained areas per home, so the structure stays O(A*k) even if
// a future catalogue puts hundreds of areas within one decay scale.
//
// SIZED BY MEASUREMENT, NOT BY GUESS, and the first value was wrong. At 16 the
// CAP bound rather than the decay floor, and the measured worst-case discarded
// mass was 3.3e-02 — a 3% hole in exactly the long-distance tail this model is
// supposed to represent, and far too large to call negligible. Rural homes take
// the 50-mile decay scale, which puts dozens of areas in genuine contention, so
// the cap has to clear that. At 64 the FLOOR binds and the discarded mass drops
// to the level reported by `worstDiscardedMass()`, which the gate asserts.
// Cost is O(A*k) on two small arrays: 71 x 64 worst case is ~4.5k entries.
inline constexpr std::size_t kMaxNearbyAreas = 64;

// The card-present distance-decay scale (MILES) for a home area. Moved here
// from `geo_pools.hpp` unchanged — same log-interpolated urbanicity proxy,
// same PROVISIONAL status, same recorded debt (population stands in for
// density because `GeoArea.landAreaKm2` was never populated).
[[nodiscard]] inline double
decayScaleMilesFor(const entity::geography::GeoArea &homeArea) noexcept {
  constexpr double kUrbanScaleMiles = 4.0;
  constexpr double kSmallTownScaleMiles = 50.0;
  constexpr double kPopUrban = 5'000'000.0;
  constexpr double kPopSmallTown = 50'000.0;

  const double p = static_cast<double>(homeArea.population);
  if (!(p > kPopSmallTown)) {
    return kSmallTownScaleMiles;
  }
  if (p >= kPopUrban) {
    return kUrbanScaleMiles;
  }
  const double t = (std::log(p) - std::log(kPopSmallTown)) /
                   (std::log(kPopUrban) - std::log(kPopSmallTown));
  return kSmallTownScaleMiles + t * (kUrbanScaleMiles - kSmallTownScaleMiles);
}

class LocalPools {
public:
  LocalPools() = default;

  // `weights` is the per-catalogue-record law to localise, aligned to
  // `catalog.records`. Pass the VOLUME weights for exploration and the REACH
  // membership weights for favourite selection — the structure is agnostic,
  // which is what lets one implementation serve both.
  //
  // Draw-free: pure arithmetic over world state, so building it moves no
  // golden by itself and the batch and windowed engines agree exactly.
  [[nodiscard]] static LocalPools
  build(const entity::merchant::Catalog &catalog,
        const entity::geography::GeoCatalog &geo,
        std::span<const entity::geography::GeoAreaId> homeAreas,
        std::span<const double> weights) {
    LocalPools out;
    if (catalog.records.empty() || geo.empty() || homeAreas.empty() ||
        weights.size() != catalog.records.size()) {
      return out;
    }

    // ---- physical merchants, grouped by area (CSR). Membership in this
    // index is a function of PLACEMENT only, never of liveness or weight, so
    // it is built once and never rebuilt.
    const auto areaCount = geo.size() + 1;
    std::vector<std::uint32_t> counts(areaCount + 1, 0);
    for (std::uint32_t i = 0; i < catalog.records.size(); ++i) {
      const auto area = catalog.records[i].location;
      if (entity::geography::validArea(area) && geo.contains(area)) {
        ++counts[area];
      }
    }
    out.areaOffsets_.assign(areaCount + 2, 0);
    for (std::size_t a = 0; a < areaCount; ++a) {
      out.areaOffsets_[a + 1] = out.areaOffsets_[a] + counts[a];
    }
    out.areaOffsets_[areaCount + 1] = out.areaOffsets_[areaCount];
    out.byArea_.assign(out.areaOffsets_[areaCount], 0);
    {
      std::vector<std::uint32_t> cursor(out.areaOffsets_.begin(),
                                        out.areaOffsets_.begin() +
                                            static_cast<std::ptrdiff_t>(areaCount));
      for (std::uint32_t i = 0; i < catalog.records.size(); ++i) {
        const auto area = catalog.records[i].location;
        if (entity::geography::validArea(area) && geo.contains(area)) {
          out.byArea_[cursor[area]++] = i;
        }
      }
    }
    if (out.byArea_.empty()) {
      return LocalPools{};
    }

    out.weights_.assign(weights.begin(), weights.end());
    out.areaCdf_.assign(out.byArea_.size(), 0.0);
    out.areaTotal_.assign(areaCount + 1, 0.0);

    // Per-area totals at ALL-LIVE, needed here so the cutoff's discarded mass
    // can be measured exactly rather than asserted from the 1e-7 floor.
    for (std::size_t a = 1; a <= geo.size(); ++a) {
      double total = 0.0;
      for (auto s = out.areaOffsets_[a]; s < out.areaOffsets_[a + 1]; ++s) {
        const double w = out.weights_[out.byArea_[s]];
        total += (w > 0.0 && std::isfinite(w)) ? w : 0.0;
      }
      out.areaTotal_[a] = total;
    }

    // ---- per home area: the retained nearby areas and their CONSTANT decay.
    out.homeToPool_.assign(areaCount + 1, -1);
    double worstDiscarded = 0.0;
    for (const auto home : homeAreas) {
      if (!entity::geography::validArea(home) || !geo.contains(home) ||
          out.homeToPool_[home] >= 0) {
        continue;
      }
      const auto &homeArea = geo.at(home);
      const double scaleMiles = decayScaleMilesFor(homeArea);

      std::vector<std::pair<double, std::uint32_t>> candidates;
      candidates.reserve(areaCount);
      for (std::size_t a = 1; a <= geo.size(); ++a) {
        if (out.areaOffsets_[a] == out.areaOffsets_[a + 1]) {
          continue; // no physical merchant is ever placed here
        }
        const auto id = static_cast<entity::geography::GeoAreaId>(a);
        const double miles =
            entity::geography::distanceMiles(homeArea, geo.at(id));
        const double decay = std::exp(-miles / scaleMiles);
        if (decay > 0.0 && std::isfinite(decay)) {
          candidates.emplace_back(decay, static_cast<std::uint32_t>(a));
        }
      }
      if (candidates.empty()) {
        continue;
      }
      std::sort(candidates.begin(), candidates.end(),
                [](const auto &lhs, const auto &rhs) {
                  // Descending decay; area id breaks ties so the retained set
                  // is content-determined and not sort-stability dependent.
                  if (lhs.first != rhs.first) {
                    return lhs.first > rhs.first;
                  }
                  return lhs.second < rhs.second;
                });
      const double floor = candidates.front().first * kDecayFloorRatio;

      out.homeToPool_[home] = static_cast<std::int32_t>(out.nearbyOffsets_.size());
      out.nearbyOffsets_.push_back(
          static_cast<std::uint32_t>(out.nearbyArea_.size()));
      double fullMass = 0.0;
      double keptMass = 0.0;
      for (const auto &[decay, area] : candidates) {
        const double mass = decay * out.areaTotal_[area];
        fullMass += mass;
        const bool capped = out.nearbyArea_.size() - out.nearbyOffsets_.back() >=
                            kMaxNearbyAreas;
        const bool belowFloor =
            decay < floor &&
            out.nearbyArea_.size() > out.nearbyOffsets_.back();
        if (capped || belowFloor) {
          continue; // counted in fullMass, deliberately not retained
        }
        out.nearbyArea_.push_back(area);
        out.nearbyDecay_.push_back(decay);
        keptMass += mass;
      }
      if (fullMass > 0.0) {
        worstDiscarded =
            std::max(worstDiscarded, (fullMass - keptMass) / fullMass);
      }
    }
    out.nearbyOffsets_.push_back(
        static_cast<std::uint32_t>(out.nearbyArea_.size()));
    out.homeCdf_.assign(out.nearbyArea_.size(), 0.0);
    out.usable_.assign(out.nearbyOffsets_.empty() ? 0
                                                  : out.nearbyOffsets_.size() - 1,
                       false);

    out.refresh(catalog, entity::merchant::kEpochMin);
    out.worstDiscarded_ = worstDiscarded;
    return out;
  }

  [[nodiscard]] bool empty() const noexcept { return byArea_.empty(); }

  [[nodiscard]] bool has(entity::geography::GeoAreaId home) const noexcept {
    if (home >= homeToPool_.size() || homeToPool_[home] < 0) {
      return false;
    }
    return usable_[static_cast<std::size_t>(homeToPool_[home])];
  }

  // Mask to the merchants live at `ts` and recompute both CDF levels.
  // O(M + A*k) — the dense predecessor was O(A*M) per month.
  void rebuildLive(const entity::merchant::Catalog &catalog,
                   std::int64_t ts) {
    refresh(catalog, ts);
  }

  // Sample a CATALOGUE index for a resident of `home`, consuming the single
  // uniform `u` for both stages by successive rescaling. Precondition: has(home).
  [[nodiscard]] std::uint32_t sample(entity::geography::GeoAreaId home,
                                     double u) const noexcept {
    const auto pool = static_cast<std::size_t>(homeToPool_[home]);
    const auto begin = nearbyOffsets_[pool];
    const auto end = nearbyOffsets_[pool + 1];

    // Stage 1: which area. Linear over <= kMaxNearbyAreas entries.
    std::size_t slot = begin;
    double lo = 0.0;
    for (; slot + 1 < end; ++slot) {
      if (u < homeCdf_[slot]) {
        break;
      }
      lo = homeCdf_[slot];
    }
    const double hi = homeCdf_[slot];

    // Stage 2: which merchant inside it, from the residual. The area CDF is
    // stored ONCE and shared by every home — the factorisation this file exists
    // for.
    const double span = hi - lo;
    const double residual = span > 0.0 ? (u - lo) / span : 0.0;
    return sampleInArea(nearbyArea_[slot],
                        residual >= 1.0 ? 0.999999999999 : residual);
  }

  // Worst-case share of a home's reachable decay-weighted mass that the
  // inter-area cutoff DISCARDS, over every built pool. A gate asserts this is
  // negligible rather than trusting the 1e-7 floor.
  [[nodiscard]] double worstDiscardedMass() const noexcept {
    return worstDiscarded_;
  }

  [[nodiscard]] std::size_t poolCount() const noexcept {
    return usable_.size();
  }

  [[nodiscard]] std::size_t memoryBytes() const noexcept {
    return byArea_.size() * sizeof(std::uint32_t) +
           areaOffsets_.size() * sizeof(std::uint32_t) +
           areaCdf_.size() * sizeof(double) +
           areaTotal_.size() * sizeof(double) +
           weights_.size() * sizeof(double) +
           nearbyArea_.size() * sizeof(std::uint32_t) +
           nearbyDecay_.size() * sizeof(double) +
           nearbyOffsets_.size() * sizeof(std::uint32_t) +
           homeCdf_.size() * sizeof(double) +
           homeToPool_.size() * sizeof(std::int32_t) + usable_.size();
  }

private:
  [[nodiscard]] std::uint32_t
  sampleInArea(std::uint32_t area, double u) const noexcept {
    const auto begin = areaOffsets_[area];
    const auto end = areaOffsets_[area + 1];
    std::size_t slot = begin;
    for (; slot + 1 < end; ++slot) {
      if (u < areaCdf_[slot]) {
        break;
      }
    }
    return byArea_[slot];
  }

  void refresh(const entity::merchant::Catalog &catalog, std::int64_t ts) {
    const bool allLive = ts == entity::merchant::kEpochMin;

    // Level 2: per-area normalised CDF over its own physical merchants.
    for (std::size_t a = 1; a + 1 < areaOffsets_.size(); ++a) {
      const auto begin = areaOffsets_[a];
      const auto end = areaOffsets_[a + 1];
      double total = 0.0;
      for (auto s = begin; s < end; ++s) {
        const auto idx = byArea_[s];
        const bool live = allLive || catalog.records[idx].liveAt(ts);
        const double w = live ? weights_[idx] : 0.0;
        total += (w > 0.0 && std::isfinite(w)) ? w : 0.0;
        areaCdf_[s] = total;
      }
      areaTotal_[a] = total;
      if (total > 0.0) {
        for (auto s = begin; s < end; ++s) {
          areaCdf_[s] /= total;
        }
      }
    }

    // Level 1: per home, a CDF over retained areas weighted by decay * total.
    for (std::size_t pool = 0; pool + 1 < nearbyOffsets_.size(); ++pool) {
      const auto begin = nearbyOffsets_[pool];
      const auto end = nearbyOffsets_[pool + 1];
      double total = 0.0;
      for (auto s = begin; s < end; ++s) {
        total += nearbyDecay_[s] * areaTotal_[nearbyArea_[s]];
        homeCdf_[s] = total;
      }
      if (!(total > 0.0) || !std::isfinite(total)) {
        usable_[pool] = false;
        continue;
      }
      usable_[pool] = true;
      for (auto s = begin; s < end; ++s) {
        homeCdf_[s] /= total;
      }
    }
  }

  // Physical merchants grouped by area. Placement-derived, never rebuilt.
  std::vector<std::uint32_t> areaOffsets_;
  std::vector<std::uint32_t> byArea_;

  // The law being localised, and the per-area normalised CDF over it.
  std::vector<double> weights_;
  std::vector<double> areaCdf_;
  std::vector<double> areaTotal_;

  // Per home pool: retained nearby areas with CONSTANT decay, plus the live
  // CDF over decay * areaTotal.
  std::vector<std::uint32_t> nearbyOffsets_;
  std::vector<std::uint32_t> nearbyArea_;
  std::vector<double> nearbyDecay_;
  std::vector<double> homeCdf_;

  std::vector<std::int32_t> homeToPool_;
  std::vector<bool> usable_;

  double worstDiscarded_ = 0.0;
};

// WHICH MERCHANTS A CARDHOLDER CAN REACH AT ALL — the favourite-membership
// sampler (merchant-selection-2026-08 step 2).
//
// Two branches, because the two acceptance modalities genuinely differ in
// reach and conflating them was the defect:
//
//   ONLINE   merchants are geography-free. An online merchant legitimately
//            reaches every cardholder in the country, so it draws from a
//            NATIONAL reach CDF. This is what keeps a real aggregator able to
//            appear on many cards — the literature says allow a small number
//            of tagged supernodes, and an online merchant is the honest place
//            for one.
//   PHYSICAL merchants are acceptance LOCATIONS with one centroid, so they
//            draw from the home area's distance-decay pool. A Phoenix outlet
//            becomes reachable only by people near Phoenix, which makes the
//            top-1 reach ceiling EMERGENT FROM GEOGRAPHY rather than a
//            declared parameter — its bound is its own area's share of the
//            population.
//
// THE SPLIT SHARE IS DERIVED, NOT DECLARED: it is the online share of live
// reach mass, read off the catalogue. That is deliberate — it preserves the
// modality mix the corpus already exports, so this change moves WHERE physical
// favourites are and not how many of them are card-present. Inventing a new
// constant here would have moved the exported `use_chip` mix at the same time
// and made two effects inseparable in one re-pin.
//
// ONE UNIFORM, same successive-rescaling discipline as `LocalPools::sample`:
// `u` selects the modality, then its residual drives the chosen branch.
class MembershipSampler {
public:
  MembershipSampler() = default;

  // `meanSetSize` is the expected favourite-row length. It is needed here and
  // not only in `buildReachModel` because THE ONLINE BRANCH HAS ITS OWN
  // EFFECTIVE DRAW COUNT, and getting that wrong put the hub straight back.
  //
  // The first version of this class flattened the reach law ONCE over the whole
  // catalogue against the full set size, then split the draws. But only
  // `onlineShare` of the draws land on the online sub-population, so the online
  // merchants were being flattened for ~30 draws while actually receiving ~3 —
  // and drawn from a much smaller pool (36 online records at pop 300 against
  // 262 total). Measured consequence: top-1 card penetration went the WRONG WAY,
  // 0.129 -> 0.361, with 31 merchants above 25%. The gate caught it.
  //
  // So the online sub-law is re-flattened against its OWN draw count,
  // `meanSetSize * onlineShare`. The physical branch needs no equivalent: its
  // concentration is bounded by geography, which is the point of the round.
  [[nodiscard]] static MembershipSampler
  build(const entity::merchant::Catalog &catalog,
        const entity::geography::GeoCatalog &geo,
        std::span<const entity::geography::GeoAreaId> homeAreas,
        std::span<const double> reachWeights, double meanSetSize,
        double cnpShare) {
    MembershipSampler out;
    out.meanSetSize_ = meanSetSize;
    out.targetOnlineShare_ = cnpShare;
    if (catalog.records.empty() || reachWeights.size() != catalog.records.size()) {
      return out;
    }
    out.weights_.assign(reachWeights.begin(), reachWeights.end());
    out.online_.assign(catalog.records.size(), false);
    for (std::size_t i = 0; i < catalog.records.size(); ++i) {
      out.online_[i] =
          catalog.records[i].footprint == entity::merchant::Footprint::online ||
          !entity::geography::validArea(catalog.records[i].location) ||
          !geo.contains(catalog.records[i].location);
    }
    out.physical_ = LocalPools::build(catalog, geo, homeAreas, reachWeights);
    out.refresh(catalog, entity::merchant::kEpochMin);
    return out;
  }

  [[nodiscard]] bool ready() const noexcept { return !onlineCdf_.empty(); }

  // `cnpShare` is re-supplied at each month boundary so the share TRACKS THE
  // CALENDAR across a multi-decade run rather than freezing at the window-start
  // value.
  void rebuildLive(const entity::merchant::Catalog &catalog, std::int64_t ts,
                   double cnpShare) {
    targetOnlineShare_ = cnpShare;
    physical_.rebuildLive(catalog, ts);
    refresh(catalog, ts);
  }

  // Sample a catalogue index for a resident of `home`, consuming ONE uniform.
  [[nodiscard]] std::uint32_t sample(entity::geography::GeoAreaId home,
                                     double u) const noexcept {
    if (u < onlineShare_) {
      const double r = onlineShare_ > 0.0 ? u / onlineShare_ : 0.0;
      return sampleOnline(r);
    }
    const double rest = 1.0 - onlineShare_;
    const double r = rest > 0.0 ? (u - onlineShare_) / rest : 0.0;
    if (physical_.has(home)) {
      return physical_.sample(home, r >= 1.0 ? 0.999999999999 : r);
    }
    // No physical merchant is reachable at all (no placement anywhere), so
    // online is the whole world. Not a fallback for "this home is remote" —
    // every home with any placed merchant gets a pool by construction.
    return sampleOnline(r);
  }

  [[nodiscard]] double onlineShare() const noexcept { return onlineShare_; }
  [[nodiscard]] const LocalPools &physical() const noexcept {
    return physical_;
  }
  [[nodiscard]] std::size_t memoryBytes() const noexcept {
    return physical_.memoryBytes() + onlineCdf_.size() * sizeof(double) +
           weights_.size() * sizeof(double) + online_.size();
  }

private:
  [[nodiscard]] std::uint32_t sampleOnline(double u) const noexcept {
    if (onlineCdf_.empty()) {
      return 0;
    }
    std::size_t slot = 0;
    for (; slot + 1 < onlineCdf_.size(); ++slot) {
      if (u < onlineCdf_[slot]) {
        break;
      }
    }
    return onlineIdx_[slot];
  }

  void refresh(const entity::merchant::Catalog &catalog, std::int64_t ts) {
    const bool allLive = ts == entity::merchant::kEpochMin;
    onlineIdx_.clear();
    onlineCdf_.clear();
    double onlineMass = 0.0;
    double physicalMass = 0.0;
    for (std::size_t i = 0; i < weights_.size(); ++i) {
      const bool live = allLive || catalog.records[i].liveAt(ts);
      const double w = live ? weights_[i] : 0.0;
      if (!(w > 0.0) || !std::isfinite(w)) {
        continue;
      }
      if (online_[i]) {
        onlineMass += w;
        onlineIdx_.push_back(static_cast<std::uint32_t>(i));
        onlineCdf_.push_back(onlineMass);
      } else {
        physicalMass += w;
      }
    }
    if (onlineMass > 0.0) {
      for (auto &c : onlineCdf_) {
        c /= onlineMass;
      }
    }
    // THE DATED TARGET, not the catalogue's own mass. A handful of online
    // merchants legitimately carry a large share of transactions — that is what
    // an aggregator IS — so the share is a behavioural fact about cardholders,
    // not an artifact of how many online records the catalogue happens to hold.
    onlineShare_ = targetOnlineShare_;
    if (!(physicalMass > 0.0) || onlineIdx_.empty()) {
      onlineShare_ = onlineIdx_.empty() ? 0.0 : 1.0;
    }
    if (!(onlineMass > 0.0)) {
      onlineShare_ = 0.0;
    }

    // RE-FLATTEN THE ONLINE SUB-LAW AGAINST ITS OWN DRAW COUNT. See the note on
    // `build` — without this the online branch is a hub factory at any
    // population where the online sub-population is small.
    const double onlineDraws = meanSetSize_ * onlineShare_;
    if (onlineIdx_.size() > 1 && onlineDraws > 0.0) {
      std::vector<double> sub;
      sub.reserve(onlineIdx_.size());
      double prev = 0.0;
      for (const double c : onlineCdf_) {
        sub.push_back(c - prev);
        prev = c;
      }
      const auto subModel = buildReachModel(sub, onlineDraws);
      if (!subModel.membership.empty()) {
        double t = 0.0;
        for (const double m : subModel.membership) {
          t += m;
        }
        if (t > 0.0) {
          double run = 0.0;
          for (std::size_t i = 0; i < onlineCdf_.size(); ++i) {
            run += subModel.membership[i] / t;
            onlineCdf_[i] = run;
          }
        }
      }
    }
  }

  LocalPools physical_;
  std::vector<double> weights_;
  std::vector<bool> online_;
  std::vector<std::uint32_t> onlineIdx_;
  std::vector<double> onlineCdf_;
  double onlineShare_ = 1.0;
  double meanSetSize_ = 1.0;
  double targetOnlineShare_ = 0.0;
};

} // namespace PhantomLedger::activity::spending::market::commerce
