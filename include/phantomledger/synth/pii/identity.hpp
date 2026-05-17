#pragma once

#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/pii/pools.hpp"
#include "phantomledger/synth/pii/samplers.hpp"

namespace PhantomLedger::synth::pii {

struct IdentityContext {
  const PoolSet *pools = nullptr;
  time::TimePoint simStart{};
  LocaleMix localeMix{};
};

} // namespace PhantomLedger::synth::pii
