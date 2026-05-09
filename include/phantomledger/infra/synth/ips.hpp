#pragma once

#include "phantomledger/entities/people.hpp"
#include "phantomledger/infra/synth/ips_output.hpp"
#include "phantomledger/infra/synth/types.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

#include <cstdint>
#include <unordered_map>

namespace PhantomLedger::infra::synth::ips {

struct AssignmentRules {
  double legitIpNoiseP = 0.05;

  double extraIpP1 = 0.35;
  double extraIpP2 = 0.10;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::between("legitIpNoiseP", legitIpNoiseP, 0.0, 1.0); });
    r.check([&] { v::between("extraIpP1", extraIpP1, 0.0, 1.0); });
    r.check([&] { v::between("extraIpP2", extraIpP2, 0.0, 1.0); });
  }

  [[nodiscard]] Output
  build(random::Rng &rng, time::Window window,
        const entity::person::Roster &people,
        const std::unordered_map<std::uint32_t, RingPlan> &ringPlans) const;
};

} // namespace PhantomLedger::infra::synth::ips
