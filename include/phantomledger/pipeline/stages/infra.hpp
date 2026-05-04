#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/infra/synth/devices.hpp"
#include "phantomledger/infra/synth/ips.hpp"
#include "phantomledger/infra/synth/rings.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/primitives/time/window.hpp"

namespace PhantomLedger::pipeline::stages::infra {

struct RoutingBehavior {
  double switchP = 0.05;
};

struct SharedInfraUse {
  double deviceP = 0.85;
  double ipP = 0.80;
};

[[nodiscard]] ::PhantomLedger::pipeline::Infra
build(::PhantomLedger::random::Rng &rng,
      const ::PhantomLedger::pipeline::Entities &entities,
      ::PhantomLedger::time::Window window,
      const ::PhantomLedger::infra::synth::rings::Config &ringBehavior = {},
      const ::PhantomLedger::infra::synth::devices::Config &deviceBehavior = {},
      const ::PhantomLedger::infra::synth::ips::Config &ipBehavior = {},
      RoutingBehavior routing = {}, SharedInfraUse shared = {});

} // namespace PhantomLedger::pipeline::stages::infra
