#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/infra/synth/devices.hpp"
#include "phantomledger/infra/synth/ips.hpp"
#include "phantomledger/infra/synth/rings.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/primitives/time/window.hpp"

namespace PhantomLedger::pipeline::stages::infra {

struct InfraSynthesis {
  ::PhantomLedger::time::Window window{};

  double routerSwitchP = 0.05;

  ::PhantomLedger::infra::synth::rings::Config ringBehavior{};
  ::PhantomLedger::infra::synth::devices::Config deviceBehavior{};
  ::PhantomLedger::infra::synth::ips::Config ipBehavior{};

  double sharedDeviceUseP = 0.85;
  double sharedIpUseP = 0.80;
};

[[nodiscard]] ::PhantomLedger::pipeline::Infra
build(::PhantomLedger::random::Rng &rng,
      const ::PhantomLedger::pipeline::Entities &entities,
      const InfraSynthesis &infra);

} // namespace PhantomLedger::pipeline::stages::infra
