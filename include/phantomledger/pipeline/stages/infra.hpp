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

class AccessInfraStage {
public:
  AccessInfraStage() = default;

  AccessInfraStage &window(::PhantomLedger::time::Window value) noexcept;
  AccessInfraStage &ringBehavior(
      const ::PhantomLedger::infra::synth::rings::Config &value) noexcept;
  AccessInfraStage &deviceBehavior(
      const ::PhantomLedger::infra::synth::devices::Config &value) noexcept;
  AccessInfraStage &
  ipBehavior(const ::PhantomLedger::infra::synth::ips::Config &value) noexcept;
  AccessInfraStage &routingBehavior(RoutingBehavior value) noexcept;
  AccessInfraStage &sharedInfra(SharedInfraUse value) noexcept;

  [[nodiscard]] ::PhantomLedger::pipeline::Infra
  build(::PhantomLedger::random::Rng &rng,
        const ::PhantomLedger::pipeline::Entities &entities,
        ::PhantomLedger::time::Window fallbackWindow) const;

private:
  [[nodiscard]] ::PhantomLedger::time::Window
  activeWindow(::PhantomLedger::time::Window fallback) const noexcept;
  void applySharedInfra(::PhantomLedger::pipeline::Infra &out) const noexcept;

  ::PhantomLedger::time::Window window_{};
  ::PhantomLedger::infra::synth::rings::Config ringBehavior_{};
  ::PhantomLedger::infra::synth::devices::Config deviceBehavior_{};
  ::PhantomLedger::infra::synth::ips::Config ipBehavior_{};
  RoutingBehavior routingBehavior_{};
  SharedInfraUse sharedInfra_{};
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
