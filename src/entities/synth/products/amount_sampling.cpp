#include "phantomledger/entities/synth/products/amount_sampling.hpp"

#include "phantomledger/primitives/random/distributions/lognormal.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"

namespace PhantomLedger::entities::synth::products {

[[nodiscard]] double samplePaymentAmount(::PhantomLedger::random::Rng &rng,
                                         double median, double sigma,
                                         double floor) {
  const double raw =
      ::PhantomLedger::probability::distributions::lognormalByMedian(
          rng, median, sigma);
  return primitives::utils::floorAndRound(raw, floor);
}

} // namespace PhantomLedger::entities::synth::products
