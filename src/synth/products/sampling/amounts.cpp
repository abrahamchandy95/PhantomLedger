#include "phantomledger/synth/products/sampling/amounts.hpp"

#include "phantomledger/primitives/random/distributions/lognormal.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"

namespace PhantomLedger::synth::products {

[[nodiscard]] double samplePaymentAmount(::PhantomLedger::random::Rng &rng,
                                         double median, double sigma,
                                         double floor) {
  const double raw =
      ::PhantomLedger::probability::distributions::lognormalByMedian(
          rng, median, sigma);
  return primitives::utils::floorAndRound(raw, floor);
}

} // namespace PhantomLedger::synth::products
