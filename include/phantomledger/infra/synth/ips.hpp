#pragma once

#include "phantomledger/entities/people.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/infra/synth/ips_output.hpp"
#include "phantomledger/infra/synth/types.hpp"
#include "phantomledger/primitives/time/window.hpp"

#include <cstdint>
#include <unordered_map>

namespace PhantomLedger::infra::synth::ips {

struct AssignmentRules {
  double legitIpNoiseP = 0.05;

  double extraIpP1 = 0.35;
  double extraIpP2 = 0.10;
};

[[nodiscard]] Output
build(random::Rng &rng, time::Window window,
      const entity::person::Roster &people,
      const std::unordered_map<std::uint32_t, RingPlan> &ringPlans,
      const AssignmentRules &rules = {});

} // namespace PhantomLedger::infra::synth::ips
