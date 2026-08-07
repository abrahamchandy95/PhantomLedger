#pragma once

/*
  VOLUME != MEMBERSHIP (FAVORITE SET) PROBABILITY

  Directly sampling a merchant's volume weight (share of total spend) to pick
  a person's favorite merchants exponentially amplifies large merchants into
  unrealistic mega-hubs.

  Instead, we construct a flatter membership distribution:
    membership q_i  ∝  w_i^γ
    realized   π_i  =  1 - (1-q_i)^k̄  (where k̄ is mean favorite set size)

  We use binary search to solve for γ so the top merchant's realized
  inclusion probability (π_i) hits a safe target (default 12%).

  Finding: 12%
  1. It aligns with real-world cardholder behavior (Krumme et al. 2013).
  2. If a merchant's inclusion rate exceeds ~25%, they become a massive graph

  WARNING: Do not alter `makeCatalog` or the base volume law here. Doing so will
  desynchronize the RNG stream and break downstream entity generation.
*/

#include <algorithm>
#include <cmath>
#include <functional>
#include <ranges>
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

/*
  Compute the membership probability distribution from the live merchant volume
  weights. `weights` is the raw volume for each merchant (dead or unborn
  merchants are 0.0), and `meanSetSize` is the expected number of distinct
  merchants a person interacts with.

  Rebuilt at every month boundary, right before the network evolution phase.
*/
[[nodiscard]] inline ReachModel
calibrateReachModel(const std::vector<double> &weights, double meanSetSize,
                    double target = kTargetTop1Reach) {
  ReachModel out;
  if (weights.empty() || !(meanSetSize > 0.0)) {
    return out;
  }

  auto isValid = [](double w) { return w > 0.0 && std::isfinite(w); };

  double total = 0.0;
  std::size_t liveCount = 0;
  double maxWeight = 0.0;
  for (const double w : weights) {
    if (!isValid(w)) {
      continue;
    }
    total += w;
    maxWeight = std::max(maxWeight, w);
    ++liveCount;
  }

  if (liveCount == 0 || !(total > 0.0)) {
    return out;
  }

  const double targetQ = reach::topProbabilityFor(target, meanSetSize);

  /*
    Calculate the market share of the biggest merchant for a given 'gamma'
    tuning dial.
      gamma = 0.0: Perfectly equal distribution
      gamma = 1.0: Weight-based distribution

    Because the top merchant's share steadily increases as gamma rises,
    we can use binary search to find the exact gamma that hits our
    target.
  */
  const auto topQFor = [&](double gamma) {
    const double sum =
        std::ranges::fold_left(weights | std::views::filter(isValid) |
                                   std::views::transform([&](double w) {
                                     return std::pow(w / total, gamma);
                                   }),
                               0.0, std::plus<>{});

    if (!(sum > 0.0)) {
      return 1.0;
    }
    return std::pow(maxWeight / total, gamma) / sum;
  };

  double gamma = 0.0;
  if (topQFor(0.0) < targetQ) {
    // If the target is reachable, use binary search to dial in the exact gamma.
    double low = 0.0;
    double high = 1.0;
    if (topQFor(1.0) <= targetQ) {
      gamma = 1.0;
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
  // gamma remains 0.0 if impossible to reach
  out.gamma = gamma;
  out.membership.assign(weights.size(), 0.0);

  double qSum = 0.0;
  for (auto [i, w] : std::views::enumerate(weights)) {
    if (!isValid(w)) {
      continue;
    }
    out.membership[i] = std::pow(w / total, gamma);
    qSum += out.membership[i];
  }

  if (!(qSum > 0.0)) {
    return ReachModel{};
  }

  for (double &m : out.membership) {
    if (!(m > 0.0)) {
      continue;
    }
    const double q = m / qSum;
    const double pi = 1.0 - std::pow(1.0 - q, meanSetSize);
    out.realizedTop1 = std::max(out.realizedTop1, pi);
  }

  return out;
}

} // namespace PhantomLedger::activity::spending::market::commerce
