#include "phantomledger/entities/synth/products/amount_sampling.hpp"

#include "phantomledger/probability/distributions/lognormal.hpp"

#include <algorithm>
#include <cmath>

namespace PhantomLedger::entities::synth::products {

[[nodiscard]] double samplePaymentAmount(::PhantomLedger::random::Rng &rng,
                                         double median, double sigma,
                                         double floor) {
  const double raw =
      ::PhantomLedger::probability::distributions::lognormalByMedian(
          rng, median, sigma);
  const double clamped = std::max(floor, raw);

  return std::round(clamped * 100.0) / 100.0;
}

} // namespace PhantomLedger::entities::synth::products
