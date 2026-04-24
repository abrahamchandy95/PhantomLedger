#pragma once

#include "phantomledger/entities/behavior/behavior.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/probability/distributions/beta.hpp"
#include "phantomledger/probability/distributions/lognormal.hpp"
#include "phantomledger/probability/distributions/normal.hpp"
#include "phantomledger/taxonomies/personas/archetypes.hpp"

#include <algorithm>

namespace PhantomLedger::entities::synth::personas {
namespace detail {

inline double perturbMedian(random::Rng &rng, double median,
                            double sigma = 0.15) {
  if (median <= 0.0) {
    return median;
  }
  return probability::distributions::lognormalByMedian(rng, median, sigma);
}

inline double perturbProb(random::Rng &rng, double p) {
  if (p <= 0.0) {
    return 0.0;
  }
  if (p >= 1.0) {
    return 1.0;
  }
  return std::clamp(probability::distributions::normal(rng, p, 0.08), 0.01,
                    0.99);
}

} // namespace detail

[[nodiscard]] inline entity::behavior::Persona
profile(random::Rng &rng, ::PhantomLedger::personas::Type type) {
  const auto arch = ::PhantomLedger::personas::archetype(type);
  const auto beta = ::PhantomLedger::personas::paycheckBeta(type);

  return entity::behavior::Persona{
      .archetype =
          entity::behavior::Archetype{
              .type = type,
              .timing = arch.timing,
              .weight = std::max(0.01, detail::perturbMedian(rng, arch.weight)),
          },
      .cash =
          entity::behavior::Cash{
              .rateMultiplier = std::max(
                  0.1, detail::perturbMedian(rng, arch.rateMultiplier)),
              .amountMultiplier = std::max(
                  0.1, detail::perturbMedian(rng, arch.amountMultiplier)),
              .initialBalance = std::max(
                  10.0, detail::perturbMedian(rng, arch.initialBalance)),
          },
      .card =
          entity::behavior::Card{
              .prob = detail::perturbProb(rng, arch.cardProb),
              .share = detail::perturbProb(rng, arch.ccShare),
              .limit =
                  std::max(200.0, detail::perturbMedian(rng, arch.creditLimit)),
          },
      .payday =
          entity::behavior::Payday{
              .sensitivity =
                  probability::distributions::beta(rng, beta.alpha, beta.beta),
          },
  };
}

} // namespace PhantomLedger::entities::synth::personas
