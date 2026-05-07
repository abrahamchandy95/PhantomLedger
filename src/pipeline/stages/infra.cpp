#include "phantomledger/pipeline/stages/infra.hpp"

#include "phantomledger/primitives/validate/checks.hpp"

#include <unordered_map>

namespace PhantomLedger::pipeline::stages::infra {

namespace {

[[nodiscard]] std::unordered_map<entity::Key, entity::PersonId>
buildOwnerMap(const entity::account::Registry &registry) {
  std::unordered_map<entity::Key, entity::PersonId> out;
  out.reserve(registry.records.size());

  for (const auto &record : registry.records) {
    if (record.owner == entity::invalidPerson) {
      continue;
    }

    out.emplace(record.id, record.owner);
  }

  return out;
}

} // namespace

AccessInfraStage &
AccessInfraStage::window(::PhantomLedger::time::Window value) noexcept {
  window_ = value;
  return *this;
}

AccessInfraStage &AccessInfraStage::ringAccess(
    ::PhantomLedger::infra::synth::rings::AccessRules value) noexcept {
  ringAccess_ = value;
  return *this;
}

AccessInfraStage &AccessInfraStage::deviceAssignment(
    ::PhantomLedger::infra::synth::devices::AssignmentRules value) noexcept {
  deviceAssignment_ = value;
  return *this;
}

AccessInfraStage &AccessInfraStage::ipAssignment(
    ::PhantomLedger::infra::synth::ips::AssignmentRules value) noexcept {
  ipAssignment_ = value;
  return *this;
}

AccessInfraStage &AccessInfraStage::routerRules(
    ::PhantomLedger::infra::RoutingRules value) noexcept {
  routerRules_ = value;
  return *this;
}

AccessInfraStage &AccessInfraStage::sharedInfra(
    ::PhantomLedger::infra::SharedInfraRules value) noexcept {
  sharedInfra_ = value;
  return *this;
}

::PhantomLedger::time::Window AccessInfraStage::activeWindow(
    ::PhantomLedger::time::Window fallback) const noexcept {
  return window_.value_or(fallback);
}

AccessInfraStage::RingPlans AccessInfraStage::buildRingPlans(
    ::PhantomLedger::random::Rng &rng, ::PhantomLedger::time::Window window,
    const ::PhantomLedger::entities::synth::people::Pack &people) const {
  return ::PhantomLedger::infra::synth::rings::build(
      rng, window, people.topology.rings, people.topology, ringAccess_);
}

::PhantomLedger::infra::synth::devices::Output AccessInfraStage::buildDevices(
    ::PhantomLedger::random::Rng &rng, ::PhantomLedger::time::Window window,
    const ::PhantomLedger::entity::person::Roster &people,
    const RingPlans &ringPlans) const {
  return ::PhantomLedger::infra::synth::devices::build(
      rng, window, people, ringPlans, deviceAssignment_);
}

::PhantomLedger::infra::synth::ips::Output AccessInfraStage::buildIps(
    ::PhantomLedger::random::Rng &rng, ::PhantomLedger::time::Window window,
    const ::PhantomLedger::entity::person::Roster &people,
    const RingPlans &ringPlans) const {
  return ::PhantomLedger::infra::synth::ips::build(rng, window, people,
                                                   ringPlans, ipAssignment_);
}

::PhantomLedger::infra::Router AccessInfraStage::buildRouter(
    const ::PhantomLedger::entity::account::Registry &accounts,
    const ::PhantomLedger::infra::synth::devices::Output &devices,
    const ::PhantomLedger::infra::synth::ips::Output &ips) const {
  return ::PhantomLedger::infra::Router::build(
      routerRules_, buildOwnerMap(accounts),
      devices.byPerson, // copy — Router owns its pools
      ips.byPerson);
}

::PhantomLedger::infra::SharedInfra AccessInfraStage::buildSharedInfra(
    const ::PhantomLedger::infra::synth::devices::Output &devices,
    const ::PhantomLedger::infra::synth::ips::Output &ips) const {
  ::PhantomLedger::infra::SharedInfra out;
  out.ringDevice = devices.ringMap;
  out.ringIp = ips.ringMap;
  out.apply(sharedInfra_);
  return out;
}

::PhantomLedger::pipeline::Infra
AccessInfraStage::build(::PhantomLedger::random::Rng &rng,
                        const ::PhantomLedger::pipeline::Entities &entities,
                        ::PhantomLedger::time::Window fallbackWindow) const {

  ::PhantomLedger::primitives::validate::Report report;
  ringAccess_.validate(report);
  deviceAssignment_.validate(report);
  ipAssignment_.validate(report);
  routerRules_.validate(report);
  sharedInfra_.validate(report);
  report.throwIfFailed();

  const auto window = activeWindow(fallbackWindow);

  ::PhantomLedger::pipeline::Infra out;
  out.ringPlans = buildRingPlans(rng, window, entities.people);
  out.devices =
      buildDevices(rng, window, entities.people.roster, out.ringPlans);
  out.ips = buildIps(rng, window, entities.people.roster, out.ringPlans);
  out.router = buildRouter(entities.accounts.registry, out.devices, out.ips);
  out.ringInfra = buildSharedInfra(out.devices, out.ips);

  return out;
}

::PhantomLedger::pipeline::Infra
build(::PhantomLedger::random::Rng &rng,
      const ::PhantomLedger::pipeline::Entities &entities,
      ::PhantomLedger::time::Window window) {
  return AccessInfraStage{}.build(rng, entities, window);
}

} // namespace PhantomLedger::pipeline::stages::infra
