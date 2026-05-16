#pragma once

#include "phantomledger/entities/infra/router.hpp"
#include "phantomledger/entities/infra/shared.hpp"
#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/pipeline/infra.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/infra/devices.hpp"
#include "phantomledger/synth/infra/ips.hpp"
#include "phantomledger/synth/infra/rings.hpp"

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace PhantomLedger::pipeline::stages::infra {

class AccessInfraStage {
public:
  AccessInfraStage() = default;

  AccessInfraStage &window(time::Window value) noexcept;
  AccessInfraStage &ringAccess(synth::infra::rings::AccessRules value) noexcept;
  AccessInfraStage &
  deviceAssignment(synth::infra::devices::AssignmentRules value) noexcept;
  AccessInfraStage &
  ipAssignment(synth::infra::ips::AssignmentRules value) noexcept;

  AccessInfraStage &
  routerRules(::PhantomLedger::infra::RoutingRules value) noexcept;
  AccessInfraStage &
  sharedInfra(::PhantomLedger::infra::SharedInfraRules value) noexcept;

  [[nodiscard]] pipeline::Infra build(random::Rng &rng,
                                      const pipeline::People &people,
                                      const pipeline::Holdings &holdings,
                                      time::Window fallbackWindow) const;

private:
  using RingPlans = std::unordered_map<std::uint32_t, synth::infra::RingPlan>;

  [[nodiscard]] time::Window activeWindow(time::Window fallback) const noexcept;

  [[nodiscard]] RingPlans
  buildRingPlans(random::Rng &rng, time::Window window,
                 const entities::synth::people::Pack &people) const;

  [[nodiscard]] synth::infra::devices::Output
  buildDevices(random::Rng &rng, time::Window window,
               const entity::person::Roster &people,
               const RingPlans &ringPlans) const;

  [[nodiscard]] synth::infra::ips::Output
  buildIps(random::Rng &rng, time::Window window,
           const entity::person::Roster &people,
           const RingPlans &ringPlans) const;

  [[nodiscard]] ::PhantomLedger::infra::Router
  buildRouter(const entity::account::Registry &accounts,
              const synth::infra::devices::Output &devices,
              const synth::infra::ips::Output &ips) const;

  [[nodiscard]] ::PhantomLedger::infra::SharedInfra
  buildSharedInfra(const synth::infra::devices::Output &devices,
                   const synth::infra::ips::Output &ips) const;

  std::optional<time::Window> window_{};
  synth::infra::rings::AccessRules ringAccess_{};
  synth::infra::devices::AssignmentRules deviceAssignment_{};
  synth::infra::ips::AssignmentRules ipAssignment_{};
  ::PhantomLedger::infra::RoutingRules routerRules_{};
  ::PhantomLedger::infra::SharedInfraRules sharedInfra_{};
};

[[nodiscard]] pipeline::Infra build(random::Rng &rng,
                                    const pipeline::People &people,
                                    const pipeline::Holdings &holdings,
                                    time::Window window);

} // namespace PhantomLedger::pipeline::stages::infra
