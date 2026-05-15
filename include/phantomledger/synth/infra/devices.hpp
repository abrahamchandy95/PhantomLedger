#pragma once

#include "phantomledger/entities/people.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/synth/infra/devices_output.hpp"
#include "phantomledger/synth/infra/types.hpp"

#include <cstdint>
#include <unordered_map>

namespace PhantomLedger::synth::infra::devices {

struct AssignmentRules {
  double legitDeviceNoiseP = 0.05;

  double secondDeviceP = 0.20;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check(
        [&] { v::between("legitDeviceNoiseP", legitDeviceNoiseP, 0.0, 1.0); });
    r.check([&] { v::between("secondDeviceP", secondDeviceP, 0.0, 1.0); });
  }

  [[nodiscard]] Output
  build(random::Rng &rng, time::Window window,
        const entity::person::Roster &people,
        const std::unordered_map<std::uint32_t, RingPlan> &ringPlans) const;
};

} // namespace PhantomLedger::synth::infra::devices
