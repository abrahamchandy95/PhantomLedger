#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/infra/synth/devices.hpp"
#include "phantomledger/infra/synth/ips.hpp"
#include "phantomledger/infra/synth/rings.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/transactions/infra/router.hpp"
#include "phantomledger/transactions/infra/shared.hpp"

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace PhantomLedger::pipeline::stages::infra {

class AccessInfraStage {
public:
  AccessInfraStage() = default;

  // All setters take by value: every rule struct here is a small POD
  // (4-32 bytes), so by-value is no more expensive than const& and gives
  // the API a single, consistent shape.
  AccessInfraStage &window(::PhantomLedger::time::Window value) noexcept;
  AccessInfraStage &
  ringAccess(::PhantomLedger::infra::synth::rings::AccessRules value) noexcept;
  AccessInfraStage &deviceAssignment(
      ::PhantomLedger::infra::synth::devices::AssignmentRules value) noexcept;
  AccessInfraStage &ipAssignment(
      ::PhantomLedger::infra::synth::ips::AssignmentRules value) noexcept;
  AccessInfraStage &
  routerRules(::PhantomLedger::infra::RoutingRules value) noexcept;
  AccessInfraStage &
  sharedInfra(::PhantomLedger::infra::SharedInfraRules value) noexcept;

  [[nodiscard]] ::PhantomLedger::pipeline::Infra
  build(::PhantomLedger::random::Rng &rng,
        const ::PhantomLedger::pipeline::Entities &entities,
        ::PhantomLedger::time::Window fallbackWindow) const;

private:
  using RingPlans = std::unordered_map<std::uint32_t,
                                       ::PhantomLedger::infra::synth::RingPlan>;

  [[nodiscard]] ::PhantomLedger::time::Window
  activeWindow(::PhantomLedger::time::Window fallback) const noexcept;

  [[nodiscard]] RingPlans buildRingPlans(
      ::PhantomLedger::random::Rng &rng, ::PhantomLedger::time::Window window,
      const ::PhantomLedger::entities::synth::people::Pack &people) const;

  [[nodiscard]] ::PhantomLedger::infra::synth::devices::Output
  buildDevices(::PhantomLedger::random::Rng &rng,
               ::PhantomLedger::time::Window window,
               const ::PhantomLedger::entity::person::Roster &people,
               const RingPlans &ringPlans) const;

  [[nodiscard]] ::PhantomLedger::infra::synth::ips::Output
  buildIps(::PhantomLedger::random::Rng &rng,
           ::PhantomLedger::time::Window window,
           const ::PhantomLedger::entity::person::Roster &people,
           const RingPlans &ringPlans) const;

  [[nodiscard]] ::PhantomLedger::infra::Router
  buildRouter(const ::PhantomLedger::entity::account::Registry &accounts,
              const ::PhantomLedger::infra::synth::devices::Output &devices,
              const ::PhantomLedger::infra::synth::ips::Output &ips) const;

  [[nodiscard]] ::PhantomLedger::infra::SharedInfra buildSharedInfra(
      const ::PhantomLedger::infra::synth::devices::Output &devices,
      const ::PhantomLedger::infra::synth::ips::Output &ips) const;

  // optional<Window> rather than Window{} so "user never called window()"
  // is distinguishable from "user passed a default-constructed Window".
  // Window's default has days == 0, which the prior sentinel collided with.
  std::optional<::PhantomLedger::time::Window> window_{};
  ::PhantomLedger::infra::synth::rings::AccessRules ringAccess_{};
  ::PhantomLedger::infra::synth::devices::AssignmentRules deviceAssignment_{};
  ::PhantomLedger::infra::synth::ips::AssignmentRules ipAssignment_{};
  ::PhantomLedger::infra::RoutingRules routerRules_{};
  ::PhantomLedger::infra::SharedInfraRules sharedInfra_{};
};

[[nodiscard]] ::PhantomLedger::pipeline::Infra
build(::PhantomLedger::random::Rng &rng,
      const ::PhantomLedger::pipeline::Entities &entities,
      ::PhantomLedger::time::Window window);

} // namespace PhantomLedger::pipeline::stages::infra
