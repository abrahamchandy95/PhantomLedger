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

  // geo-causal-v1: the raw run seed. Home geography is placed per
  // HOUSEHOLD from the EMBEDDED catalogue (synth::geo::geography()) on
  // named RNG lanes derived from this seed — the same seed the transfer
  // stage feeds the family household partition, so the home grouping is
  // byte-identical to the family graph's households (coresidents share
  // one address) while never perturbing the shared entity stream.
  std::uint64_t worldSeed = 0;
};

} // namespace PhantomLedger::synth::pii
