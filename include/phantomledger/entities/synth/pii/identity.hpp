#pragma once

#include "phantomledger/entities/synth/pii/pools.hpp"
#include "phantomledger/entities/synth/pii/samplers.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

namespace PhantomLedger::entities::synth::pii {

struct IdentityContext {
  const PoolSet *pools = nullptr;
  time::TimePoint simStart{};
  LocaleMix localeMix{};
};

} // namespace PhantomLedger::entities::synth::pii
