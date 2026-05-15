#pragma once

#include "phantomledger/entities/people.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/synth/infra/types.hpp"

#include <cstdint>
#include <span>
#include <unordered_map>

namespace PhantomLedger::synth::infra::rings {

struct AccessRules {
  std::int32_t burstDaysMin = 7;
  std::int32_t burstDaysMax = 30;

  double sharedDeviceP = 0.70;
  double sharedIpP = 0.60;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::nonNegative("burstDaysMin", burstDaysMin); });
    r.check([&] { v::ge("burstDaysMax", burstDaysMax, burstDaysMin); });
    r.check([&] { v::between("sharedDeviceP", sharedDeviceP, 0.0, 1.0); });
    r.check([&] { v::between("sharedIpP", sharedIpP, 0.0, 1.0); });
  }

  [[nodiscard]] std::unordered_map<std::uint32_t, RingPlan>
  build(random::Rng &rng, time::Window window,
        std::span<const entity::person::Ring> rings,
        const entity::person::Topology &topology) const;
};

} // namespace PhantomLedger::synth::infra::rings
