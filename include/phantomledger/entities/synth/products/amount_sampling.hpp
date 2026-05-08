#pragma once

#include "phantomledger/primitives/random/rng.hpp"

namespace PhantomLedger::entities::synth::products {

[[nodiscard]] double samplePaymentAmount(::PhantomLedger::random::Rng &rng,
                                         double median, double sigma,
                                         double floor);

} // namespace PhantomLedger::entities::synth::products
