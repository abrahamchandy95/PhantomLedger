#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/infra/synth/devices.hpp"
#include "phantomledger/infra/synth/ips.hpp"
#include "phantomledger/infra/synth/rings.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/primitives/time/window.hpp"

namespace PhantomLedger::pipeline::stages::infra {

struct Inputs {
  ::PhantomLedger::time::Window window{};

  double switchP = 0.05;

  ::PhantomLedger::infra::synth::rings::Config ringsCfg{};
  ::PhantomLedger::infra::synth::devices::Config devicesCfg{};
  ::PhantomLedger::infra::synth::ips::Config ipsCfg{};

  double useSharedDeviceP = 0.85;
  double useSharedIpP = 0.80;
};

[[nodiscard]] ::PhantomLedger::pipeline::Infra
build(::PhantomLedger::random::Rng &rng,
      const ::PhantomLedger::pipeline::Entities &entities, const Inputs &in);

} // namespace PhantomLedger::pipeline::stages::infra
