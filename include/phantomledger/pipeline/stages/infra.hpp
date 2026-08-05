#pragma once

#include "phantomledger/entities/infra/router.hpp"
#include "phantomledger/entities/infra/shared.hpp"
#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/pipeline/infra.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/infra/attackers.hpp"
#include "phantomledger/synth/infra/devices.hpp"
#include "phantomledger/synth/infra/ips.hpp"
#include "phantomledger/synth/infra/rings.hpp"

#include <optional>

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

  AccessInfraStage &
  attackerAssignment(synth::infra::attackers::AssignmentRules value) noexcept;

  // `runSeed` drives ONE isolated lane: the attacker-infrastructure pool
  // (attacker-infra-2026-07). It is deliberately NOT taken off `rng`.
  //
  // The shared sequential stream is consumed by ring access, then
  // devices, then IPs, and then by everything downstream of the world
  // build — so appending a fourth consumer here would shift every
  // legitimate draw in the corpus for a change that has no business
  // touching amounts, timestamps or row counts. Isolating it is the same
  // device `buildMerchants` and `issueCreditCards` already use for their
  // seed-driven lanes, and it is what keeps this round's golden movement
  // auditable: the corpus moves only where an attacker endpoint is
  // actually written.
  [[nodiscard]] pipeline::Infra build(random::Rng &rng,
                                      const pipeline::People &people,
                                      const pipeline::Holdings &holdings,
                                      time::Window fallbackWindow,
                                      std::uint64_t runSeed = 0) const;

private:
  [[nodiscard]] ::PhantomLedger::infra::SharedInfra
  buildSharedInfra(const synth::infra::devices::Output &devices,
                   const synth::infra::ips::Output &ips) const;

  std::optional<time::Window> window_{};
  synth::infra::rings::AccessRules ringAccess_{};
  synth::infra::devices::AssignmentRules deviceAssignment_{};
  synth::infra::ips::AssignmentRules ipAssignment_{};
  ::PhantomLedger::infra::RoutingRules routerRules_{};
  ::PhantomLedger::infra::SharedInfraRules sharedInfra_{};
  synth::infra::attackers::AssignmentRules attackerAssignment_{};
};

} // namespace PhantomLedger::pipeline::stages::infra
