#pragma once

#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/pii/pools.hpp"
#include "phantomledger/synth/pii/samplers.hpp"

#include <cstdint>

namespace PhantomLedger::synth::pii {

struct IdentityContext {
  const PoolSet *pools = nullptr;
  time::TimePoint simStart{};
  LocaleMix localeMix{};

  // geo-causal-v1: seed for the isolated {"home-geo", …} RNG lane. Home
  // geography is drawn from the EMBEDDED catalogue (synth::geo::
  // geography()); only the lane seed is per-run, so home placement is
  // deterministic yet never perturbs the shared entity stream.
  std::uint64_t geoSeed = 0;
};

} // namespace PhantomLedger::synth::pii
