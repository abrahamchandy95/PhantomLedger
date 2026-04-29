#pragma once

#include "phantomledger/entities/people.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/infra/synth/devices_output.hpp"
#include "phantomledger/infra/synth/types.hpp"
#include "phantomledger/primitives/time/window.hpp"

#include <cstdint>
#include <unordered_map>

namespace PhantomLedger::infra::synth::devices {

struct Config {
  double legitDeviceNoiseP = 0.05;

  double secondDeviceP = 0.20;
};

[[nodiscard]] Output
build(random::Rng &rng, time::Window window,
      const entity::person::Roster &people,
      const std::unordered_map<std::uint32_t, RingPlan> &ringPlans,
      const Config &cfg = {});

} // namespace PhantomLedger::infra::synth::devices
