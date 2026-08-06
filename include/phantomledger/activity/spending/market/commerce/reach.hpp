#pragma once

/*
  REACH IS NOT VOLUME, and the two must never be sampled from one law.

  `Record.weight` is a VOLUME weight — the share of card spend a merchant
  should receive. Favourite-set MEMBERSHIP is a graph EDGE, and a picker
  drawing favK ~ U[8,30] times turns a volume weight `w` into

      P(card -> merchant) = 1 - (1 - w)^favK

  a favK-fold amplification of volume into edge probability. Measured at
  8,000 people: a ~5% volume weight became a 51% share of every card, which
  the monthly evolver's ratchet took to 85%. It deflated volume at the same
  time — the within-row pick was uniform, so realized top-1 volume was 2.2%
  against its own 2.56% nominal weight. Inflated degree and deflated volume
  cancel in every total, which is why the symptom presented as "too few
  merchants" while the merchant count was correct.

  THE CONSTRUCTION — a separate, flatter, draw-free law for membership:

      membership q_i  ∝  w_i^γ          what the favourite draw samples
      realized   π_i  =  1 - (1-q_i)^k̄  inclusion probability at the mean
                                        set size

  γ is solved by bisection so max_i π_i equals a declared target. Volume
  within a card is governed separately by `commerce/affinity.hpp` (a Zipf
  rank law over the person's own set), and the two compose.

  DO NOT RE-IMPOSE NATIONAL VOLUME CONCENTRATION ON TOP OF REACH. Since
  national share_i ≈ reach_i × (that merchant's share of a card's visits),
  reach pinned at 0.12 against a measured ~13-19% within-card top-1 caps any
  outlet's national share at ~0.12 × 0.19 ≈ 2%. Weighting the within-row
  pick by w_i/π_i restores E[share] = w_i exactly, but buys the higher
  national share only by pushing the WITHIN-card top-1 share to 31%, outside
  Krumme's cited 13% (NA) to 22% (EU) band — it breaks a cited quantity to
  chase an unreachable one. The 6.66% US top-1 figure (NRF Walmart / Census
  MARTS 2024) is a BRAND number reachable only at BRAND reach: 0.86
  household penetration × ~8% within-household share ≈ 6.9%. At outlet
  granularity a 12%-reach merchant must sit near 1% national share, and it
  does — measured 0.64% at n=570 and 0.48% at n=26,000. Pinning both is
  over-determined; the two CITED quantities are pinned and national
  concentration falls out, PRINTED by `test_card_merchant_graph`.

  TARGET LEVEL 0.12, CLASS S UNCITED, because no published source reports
  per-card merchant reach. Krumme et al. give the card side only (median 64
  distinct merchants per cardholder per 6 months, Zipf α = 0.80, top-1 = 13%
  of that cardholder's visits; Scientific Reports 3:1645, 2013). Numerator's
  household-penetration ladder (Great Value 86% of US households, McDonald's
  87%, Amazon 83%) is BRAND granularity and the wrong anchor: every
  non-online record holds exactly one GeoArea and the exporter writes one
  `cf_Merchant_Location` centroid per record, so a Record is an ACCEPTANCE
  LOCATION and cannot reach half a national cardholder population.

  The 5-15% band is anchored on the consequence, not a measurement: above
  ~25% the merchant is a hub through which a large fraction of card PAIRS
  are 2-hop neighbours, "shared merchant" carries ~0 bits, and the
  common-point-of-purchase motif card-fraud graph models exist to find stops
  existing. R-GCN's authors record that its fixed 1/|N_i^r| normalisation is
  "particularly problematic for nodes of high degree" (Schlichtkrull et al.,
  ESWC 2018, §5.1).

  THIS FILE MUST NOT TOUCH `makeCatalog`. The volume law is still a fixed-σ
  lognormal whose top-1 share depends on catalogue size (6.05% at n=570
  falling to 0.79% at n=26,000, against a size-INVARIANT 6.66% US anchor).
  Replacing it with a rank-size law needs its own round: making `coreCount`
  population-independent changes `makeCatalog`'s SHARED-ENTITY-STREAM draw
  count and desynchronises every downstream entity value — 51,079
  `test_membership` violations, measured. This file rides the market
  bootstrap's own seed and the `{"payees", id}` lane, so the entity layer is
  untouched.
 */

#include <cmath>
#include <cstddef>
#include <vector>

namespace PhantomLedger::activity::spending::market::commerce {

/* Declared target for the most-reachable merchant's share of cards. */
inline constexpr double kTargetTop1Reach = 0.12;

/* Hard ceiling the gate enforces; above this the hub destroys the
 * shared-merchant signal. */
inline constexpr double kMaxTop1Reach = 0.25;

struct ReachModel {
  /* Sampling weights for favourite-set MEMBERSHIP, aligned to the catalogue.
   * Zero for merchants not live at the rebuild instant. */
  std::vector<double> membership;

  /* The solved flattening exponent and the realized top-1 inclusion
   * probability it achieves. Both PRINTED by the gate: `gamma` at the
   * bisection floor means the target was unreachable and membership fell
   * back to uniform — a real condition at small catalogues rather than a
   * bug, detected by `reachAchievable` below. */
  double gamma = 1.0;
  double realizedTop1 = 0.0;

  [[nodiscard]] bool empty() const noexcept { return membership.empty(); }
};

namespace reach {

/* The membership-CDF probability the top merchant must carry for its
 * realized inclusion probability over `k` draws to equal `target`. Inverts
 * pi = 1 - (1 - q)^k. */
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

/* Is the target reachable at all? The flattest possible membership law is
 * uniform over the L live merchants, which still gives every merchant an
 * inclusion probability of 1 - (1 - 1/L)^k. When that already exceeds the
 * target no exponent can meet it, and reporting so is the correct response
 * rather than widening a band.
 *
 * MEAN REACH IS AN IDENTITY — (distinct merchants per card) / (live
 * merchants) — so some configurations cannot be fixed at any parameter
 * setting. At 8,000 people over 20 years the live catalogue ends at ~439
 * merchants while a realistic 20-year cardholder touches ~460 distinct
 * merchants, putting mean reach above 1 with NO achievable top-1 ceiling.
 * 500,000 people over 2 years is coherent: ~110 distinct per card against
 * ~26,000 live merchants is a mean reach of 0.4%. */
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

/* Build the membership law from the live volume weights. `weights` is the
 * live-masked volume vector (zeros for dead or unborn records),
 * `meanSetSize` the expected favourite-row length.
 *
 * MUST STAY DRAW-FREE AND STATELESS: the bisection reads only `weights`, so
 * the batch and windowed engines compute the identical model at the
 * identical instant. Rebuilt at each month boundary beside the live CDFs and
 * BEFORE `evolveAll`, because the evolver's add pass samples this model. */
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

  /* max_i q_i as a function of gamma, with q_i proportional to w_i^gamma.
   * Monotone INCREASING in gamma: gamma = 0 is uniform (1/liveCount) and
   * gamma = 1 is the raw volume law (q == w). */
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
    /* Reachable: bisect. 60 iterations resolves gamma to ~1e-18, far past
     * any level the realized share can distinguish, and the loop count must
     * stay FIXED so it cannot become data-dependent. */
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
  /* gamma stays 0 (uniform membership) when the target is unreachable.
   * `reach::reachAchievable` is how a caller or a gate detects that, and
   * `ReachModel::gamma` is printed so it can never be silent. */

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
