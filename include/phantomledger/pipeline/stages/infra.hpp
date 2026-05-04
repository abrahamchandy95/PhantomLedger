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
  AccessInfraStage &ringAccess(
      const ::PhantomLedger::infra::synth::rings::AccessRules &value) noexcept;
  AccessInfraStage &
  deviceAssignment(const ::PhantomLedger::infra::synth::devices::AssignmentRules
                       &value) noexcept;
  AccessInfraStage &
  ipAssignment(const ::PhantomLedger::infra::synth::ips::AssignmentRules
                   &value) noexcept;
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
  ::PhantomLedger::infra::synth::rings::AccessRules ringAccess_{};
  ::PhantomLedger::infra::synth::devices::AssignmentRules deviceAssignment_{};
  ::PhantomLedger::infra::synth::ips::AssignmentRules ipAssignment_{};
  RoutingBehavior routingBehavior_{};
  SharedInfraUse sharedInfra_{};
};

[[nodiscard]] ::PhantomLedger::pipeline::Infra
build(::PhantomLedger::random::Rng &rng,
      const ::PhantomLedger::pipeline::Entities &entities,
      ::PhantomLedger::time::Window window,
      const ::PhantomLedger::infra::synth::rings::AccessRules &ringAccess = {},
      const ::PhantomLedger::infra::synth::devices::AssignmentRules
          &deviceAssignment = {},
      const ::PhantomLedger::infra::synth::ips::AssignmentRules &ipAssignment =
          {},
      RoutingBehavior routing = {}, SharedInfraUse shared = {});

} // namespace PhantomLedger::pipeline::stages::infra
