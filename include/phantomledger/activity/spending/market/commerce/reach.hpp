#pragma once
//
// phantomledger/activity/spending/market/commerce/reach.hpp
//
// REACH IS NOT VOLUME (merchant-selection-2026-08).
//
// ===================================================================
// THE DEFECT
//
// `Record.weight` is a VOLUME weight — the share of card spend a merchant
// should receive. It was being used, unchanged, as the sampling law for
// FAVOURITE-SET MEMBERSHIP (`bootstrap.cpp`, `picker.pick(rng, startLiveCdf,
// favK, ...)`), and membership is a graph EDGE. Because the picker draws
// favK ~ U[8,30] times, a volume weight `w` reached the graph as
//
//     P(card -> merchant) = 1 - (1 - w)^favK
//
// which is a **favK-fold amplification of a volume weight into an edge
// probability**. At the owner's 8,000-person run the top merchant's ~5%
// volume weight became a 51% share of every card in the corpus, measured;
// the monthly evolver's ratchet then took it to 85%.
//
// The damage ran the other way too. Once membership is decided, the pick
// inside the row was UNIFORM, so `w` had no further influence and the
// realized top-1 share of transaction VOLUME came out at 2.2% against its
// own 2.56% nominal weight. **One bug inflated degree and deflated volume
// at the same time**, which is why no aggregate looked wrong and why the
// symptom presented as "too few merchants" when the merchant count was
// correct all along.
//
// ===================================================================
// THE CONSTRUCTION
//
// A separate, flatter law for MEMBERSHIP, draw-free:
//
//   membership q_i  ∝  w_i^γ                  -- what the favourite draw samples
//   realized   π_i  =  1 - (1 - q_i)^k̄        -- the inclusion probability it
//                                                achieves at the mean set size
//
// γ is solved by bisection so that max_i π_i equals a DECLARED target. Reach
// therefore stops being an emergent 0.51 and becomes a pinned parameter.
// Volume within a card is then governed by `commerce/affinity.hpp` (a Zipf
// rank law over the person's own set), and the two compose.
//
// ===================================================================
// WHY NATIONAL VOLUME CONCENTRATION IS NOW EMERGENT AND MEASURED, NOT IMPOSED
//
// The first version of this file also carried a `volumeRatio` vector,
// v_i = w_i/π_i, on the reasoning that weighting the within-row pick by it
// restores E[share of i] = w_i exactly and so returns the calibrated
// popularity law to VOLUME while `q` governs REACH. That algebra is right
// and the design is still wrong, for a reason only the arithmetic shows:
//
//   national share of i  ≈  reach_i × (that merchant's share of a card's
//                                      own visits)
//
// With reach pinned at 0.12 and a cardholder's top merchant taking the
// measured ~13-19% of their visits, the largest national share ANY outlet
// can reach is ~0.12 × 0.19 ≈ 2%. Stacking v_i on top of the affinity law
// bought a higher national share only by pushing the WITHIN-card top-1
// share to 31%, well outside Krumme's measured 13% (NA) to 22% (EU) band —
// it over-concentrated the one quantity that is actually cited in order to
// hit a target that is not achievable at this granularity.
//
// **The 6.66% US top-1 figure (NRF Walmart / Census MARTS 2024) is a BRAND
// number and it is only reachable at BRAND reach**: 0.86 household
// penetration × ~8% within-household share ≈ 6.9%. At the outlet
// granularity this generator uses, an outlet with 12% reach must have a
// national share near 1%, and it does — measured 0.64% at n=570 and 0.48%
// at n=26,000. Pinning both reach and national volume is OVER-DETERMINED;
// the model pins the two CITED quantities (reach ceiling, within-card Zipf)
// and lets national concentration fall out. `test_card_merchant_graph`
// PRINTS the emergent curve and bands only its shape, which is the honest
// form for a quantity no longer being imposed.
//
// ===================================================================
// TARGET LEVEL
//
// 0.12 — the midpoint of a 5-15% band for top-1 merchant card penetration
// at OUTLET granularity, with a hard ceiling of 0.25.
//
// CLASS S UNCITED at the level, and the reason is worth recording: **no
// published source reports per-card merchant reach.** Krumme et al. give
// the card side only (median 64 distinct merchants per cardholder per 6
// months, Zipf α = 0.80, top-1 = 13% of that cardholder's visits;
// Scientific Reports 3:1645, 2013). Numerator's household-penetration
// ladder — Walmart's Great Value at 86% of US households in one year,
// McDonald's 87%, Amazon 83% — is BRAND granularity and is the wrong
// anchor here: `place.hpp` gives every non-online record exactly one
// GeoArea and the exporter writes one `cf_Merchant_Location` centroid per
// record, so a Record is an ACCEPTANCE LOCATION, as
// `entities/counterparties/merchants.hpp` states outright. A single outlet
// cannot reach half a national cardholder population.
//
// The 5-15% band is anchored on the consequence rather than a measurement:
// above ~25% the merchant becomes a hub through which a large fraction of
// card PAIRS are 2-hop neighbours, "shared merchant" carries ~0 bits, and
// the common-point-of-purchase motif that card-fraud graph models exist to
// find stops existing. R-GCN's own authors record that its fixed
// 1/|N_i^r| normalisation is "particularly problematic for nodes of high
// degree" (Schlichtkrull et al., ESWC 2018, §5.1).
//
// ===================================================================
// WHAT THIS FILE DELIBERATELY DOES NOT DO
//
// It does not touch `makeCatalog`. The volume law itself is still a
// fixed-σ lognormal, whose top-1 share is a function of the catalogue size
// (measured 6.05% at n=570 falling to 0.79% at n=26,000, against a US
// anchor of 6.66% that should be size-INVARIANT). Replacing it with a
// rank-size law is a separate round because making `coreCount`
// population-independent changes `makeCatalog`'s SHARED-ENTITY-STREAM draw
// count, which desynchronises every downstream entity value — the
// documented 51,079 `test_membership` violations. This file rides the
// market bootstrap's own seed and the `{"payees", id}` lane, so the entity
// layer is untouched and `test_membership` / `test_lifespan` /
// `test_arch_equivalence` staying green is this round's canary.
//

#include <cmath>
#include <cstddef>
#include <vector>

namespace PhantomLedger::activity::spending::market::commerce {

// Declared target for the most-reachable merchant's share of cards.
inline constexpr double kTargetTop1Reach = 0.12;

// Hard ceiling the gate enforces; above this the hub destroys the
// shared-merchant signal (see the header note).
inline constexpr double kMaxTop1Reach = 0.25;

struct ReachModel {
  // Sampling weights for favourite-set MEMBERSHIP, aligned to the
  // catalogue. Zero for merchants not live at the rebuild instant.
  std::vector<double> membership;

  // The solved flattening exponent, and the realized top-1 inclusion
  // probability it achieves. Both PRINTED by the gate: `gamma` at the
  // bisection floor means the target was unreachable and reach fell back to
  // uniform, which is a real condition at small catalogues rather than a
  // bug — see `reachAchievable` below.
  double gamma = 1.0;
  double realizedTop1 = 0.0;

  [[nodiscard]] bool empty() const noexcept { return membership.empty(); }
};

namespace reach {

// The membership-CDF probability the top merchant must carry for its
// realized inclusion probability over `k` draws to equal `target`.
// Inverts pi = 1 - (1 - q)^k.
[[nodiscard]] inline double topProbabilityFor(double target,
                                              double k) noexcept {
  if (!(k > 0.0) || !(target > 0.0)) {
    return 0.0;
  }
  if (target >= 1.0) {
    return 1.0;
  }
  return 1.0 - std::pow(1.0 - target, 1.0 / k);
}

// IS THE TARGET REACHABLE AT ALL? The flattest possible membership law is
// uniform over the L live merchants, which still gives every merchant an
// inclusion probability of 1 - (1 - 1/L)^k. When that already exceeds the
// target, no exponent can meet it: the catalogue is too small for the set
// size, and the honest response is to say so rather than to widen a band.
//
// This is the arithmetic behind a limitation worth registering: mean reach
// is (distinct merchants per card) / (live merchants), so a configuration
// that wants realistic per-cardholder breadth out of a small catalogue has
// mean reach near or above 1 and NO top-1 ceiling is achievable in it. At
// 8,000 people over 20 years the live catalogue ends at ~439 merchants
// while a realistic 20-year cardholder touches ~460 distinct merchants.
// The target configuration (500,000 people, 2 years) is coherent:
// ~110 distinct per card against ~26,000 live merchants is a mean reach of
// 0.4%.
[[nodiscard]] inline bool reachAchievable(std::size_t liveCount, double k,
                                          double target) noexcept {
  if (liveCount == 0 || !(k > 0.0)) {
    return false;
  }
  const double uniform =
      1.0 - std::pow(1.0 - 1.0 / static_cast<double>(liveCount), k);
  return uniform <= target;
}

} // namespace reach

// Build the membership law and the volume-restoring ratio from the live
// volume weights.
//
// DRAW-FREE AND STATELESS. The bisection reads only `weights`, so the batch
// and windowed engines compute the identical model at the identical instant
// — the `attacker-infra` lockstep requirement. Rebuilt at each month
// boundary beside the live CDFs, and BEFORE `evolveAll`, for the same
// reason the home refresh is (`relocation` rule 6): the evolver's add pass
// samples this model.
//
// `weights` is the live-masked volume vector (zeros for dead or unborn
// records), `meanSetSize` the expected favourite-row length.
[[nodiscard]] inline ReachModel
buildReachModel(const std::vector<double> &weights, double meanSetSize,
                double target = kTargetTop1Reach) {
  ReachModel out;
  if (weights.empty() || !(meanSetSize > 0.0)) {
    return out;
  }

  double total = 0.0;
  std::size_t liveCount = 0;
  double maxWeight = 0.0;
  for (const double w : weights) {
    if (!(w > 0.0) || !std::isfinite(w)) {
      continue;
    }
    total += w;
    maxWeight = w > maxWeight ? w : maxWeight;
    ++liveCount;
  }
  if (liveCount == 0 || !(total > 0.0)) {
    return out;
  }

  const double targetQ = reach::topProbabilityFor(target, meanSetSize);

  // max_i q_i as a function of gamma, with q_i proportional to w_i^gamma.
  // Monotone INCREASING in gamma: gamma = 0 is uniform (1/liveCount) and
  // gamma = 1 reproduces the pre-round behaviour (q == w).
  const auto topQFor = [&](double gamma) {
    double sum = 0.0;
    for (const double w : weights) {
      if (w > 0.0 && std::isfinite(w)) {
        sum += std::pow(w / total, gamma);
      }
    }
    if (!(sum > 0.0)) {
      return 1.0;
    }
    return std::pow(maxWeight / total, gamma) / sum;
  };

  double gamma = 0.0;
  if (topQFor(0.0) < targetQ) {
    // Reachable: bisect. 60 iterations resolves gamma to ~1e-18, far past
    // any level the realized share can distinguish, and the loop count is
    // FIXED so it cannot become data-dependent.
    double low = 0.0;
    double high = 1.0;
    if (topQFor(1.0) <= targetQ) {
      gamma = 1.0; // even the raw volume law is flatter than the target
    } else {
      for (int i = 0; i < 60; ++i) {
        const double mid = 0.5 * (low + high);
        if (topQFor(mid) < targetQ) {
          low = mid;
        } else {
          high = mid;
        }
      }
      gamma = 0.5 * (low + high);
    }
  }
  // gamma stays 0 (uniform membership) when the target is unreachable —
  // `reach::reachAchievable` is how a caller or a gate detects that, and
  // `ReachModel::gamma` is printed so it can never be silent.

  out.gamma = gamma;
  out.membership.assign(weights.size(), 0.0);

  double qSum = 0.0;
  for (std::size_t i = 0; i < weights.size(); ++i) {
    const double w = weights[i];
    if (!(w > 0.0) || !std::isfinite(w)) {
      continue;
    }
    out.membership[i] = std::pow(w / total, gamma);
    qSum += out.membership[i];
  }
  if (!(qSum > 0.0)) {
    return ReachModel{};
  }

  for (std::size_t i = 0; i < weights.size(); ++i) {
    if (!(out.membership[i] > 0.0)) {
      continue;
    }
    const double q = out.membership[i] / qSum;
    const double pi = 1.0 - std::pow(1.0 - q, meanSetSize);
    out.realizedTop1 = pi > out.realizedTop1 ? pi : out.realizedTop1;
  }

  return out;
}

} // namespace PhantomLedger::activity::spending::market::commerce
