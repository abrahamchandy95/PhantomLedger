#pragma once

#include "phantomledger/entities/people.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/infra/synth/types.hpp"
#include "phantomledger/primitives/time/window.hpp"

#include <cstdint>
#include <span>
#include <unordered_map>

namespace PhantomLedger::infra::synth::rings {

struct AccessRules {
  std::int32_t burstDaysMin = 7;
  std::int32_t burstDaysMax = 30;

  double sharedDeviceP = 0.70;
  double sharedIpP = 0.60;
};

[[nodiscard]] std::unordered_map<std::uint32_t, RingPlan>
build(random::Rng &rng, time::Window window,
      std::span<const entity::person::Ring> rings,
      const entity::person::Topology &topology, const AccessRules &rules = {});

} // namespace PhantomLedger::infra::synth::rings
