#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/synth/people/fraud.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/probability/distributions/beta.hpp"
#include "phantomledger/probability/distributions/lognormal.hpp"
#include "phantomledger/probability/distributions/poisson.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace PhantomLedger::entities::synth::people {

struct TempRing {
  std::vector<entity::PersonId> members;
  std::vector<entity::PersonId> frauds;
  std::vector<entity::PersonId> mules;
  std::vector<entity::PersonId> victims;
};

[[nodiscard]] inline int sampleRingCount(random::Rng &rng, const Fraud &cfg,
                                         int population) {
  if (cfg.rings.perTenKMean <= 0.0 || population <= 0) {
    return 0;
  }

  double rate = cfg.rings.perTenKMean;
  if (cfg.rings.perTenKSigma > 0.0) {
    const double sigma = cfg.rings.perTenKSigma;
    const double mu = std::log(rate) - (sigma * sigma) / 2.0;
    rate = probability::distributions::lognormal(rng, mu, sigma);
  }

  return std::max(0, static_cast<int>(std::round(
                         rate * (static_cast<double>(population) / 10000.0))));
}

[[nodiscard]] inline bool sampleRing(random::Rng &rng, const Fraud &cfg,
                                     int remainingBudget, int eligibleVictims,
                                     RingPlan &out) {
  if (remainingBudget < cfg.size.min) {
    return false;
  }

  int ringSize = static_cast<int>(std::round(
      probability::distributions::lognormal(rng, cfg.size.mu, cfg.size.sigma)));
  ringSize = std::clamp(ringSize, cfg.size.min, cfg.size.max);
  ringSize = std::min(ringSize, remainingBudget);
  if (ringSize < cfg.size.min) {
    return false;
  }

  double muleFrac =
      probability::distributions::beta(rng, cfg.mules.alpha, cfg.mules.beta);
  muleFrac = std::clamp(muleFrac, cfg.mules.minFrac, cfg.mules.maxFrac);

  int muleCount =
      std::max(1, static_cast<int>(std::round(muleFrac * ringSize)));
  muleCount = std::min(muleCount, ringSize - 1);

  int victimCount =
      static_cast<int>(std::round(probability::distributions::lognormal(
          rng, cfg.victims.mu, cfg.victims.sigma)));
  victimCount = std::clamp(victimCount, cfg.victims.min, cfg.victims.max);
  victimCount = std::min(victimCount, std::max(0, eligibleVictims));

  out = RingPlan{.size = ringSize, .mules = muleCount, .victims = victimCount};
  return true;
}

[[nodiscard]] inline int sampleSolos(random::Rng &rng, const Fraud &cfg,
                                     int population, int remainingBudget) {
  const double rate =
      cfg.solos.perTenK * (static_cast<double>(population) / 10000.0);
  if (rate <= 0.0) {
    return 0;
  }

  const int count =
      static_cast<int>(probability::distributions::poisson(rng, rate));
  return std::min(count, std::max(0, remainingBudget));
}

[[nodiscard]] inline bool hasRing(const std::vector<std::uint32_t> &rings,
                                  std::uint32_t ringId) noexcept {
  return std::find(rings.begin(), rings.end(), ringId) != rings.end();
}

inline void injectMultiRingMules(random::Rng &rng, const Fraud &cfg,
                                 std::vector<TempRing> &rings,
                                 std::vector<std::vector<std::uint32_t>> &home,
                                 entity::PersonId people) {
  const auto ringCount = static_cast<std::uint32_t>(rings.size());
  if (ringCount < 2) {
    return;
  }

  for (entity::PersonId mule = 1; mule <= people; ++mule) {
    auto &homes = home[mule];
    if (homes.empty() || !rng.coin(cfg.mules.multiRingP) ||
        homes.size() >= ringCount) {
      continue;
    }

    if (homes.size() == 1) {
      const auto blocked = homes[0];
      std::uint32_t pick =
          static_cast<std::uint32_t>(rng.choiceIndex(ringCount - 1));
      if (pick >= blocked) {
        ++pick;
      }

      rings[pick].mules.push_back(mule);
      homes.push_back(pick);
      continue;
    }

    std::vector<std::uint32_t> candidates;
    candidates.reserve(ringCount - static_cast<std::uint32_t>(homes.size()));
    for (std::uint32_t ringId = 0; ringId < ringCount; ++ringId) {
      if (!hasRing(homes, ringId)) {
        candidates.push_back(ringId);
      }
    }

    if (candidates.empty()) {
      continue;
    }

    const auto target = candidates[rng.choiceIndex(candidates.size())];
    rings[target].mules.push_back(mule);
    homes.push_back(target);
  }
}

} // namespace PhantomLedger::entities::synth::people
