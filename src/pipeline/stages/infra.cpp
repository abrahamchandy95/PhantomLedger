#include "phantomledger/pipeline/stages/infra.hpp"

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

AccessInfraStage &AccessInfraStage::ringBehavior(
    const ::PhantomLedger::infra::synth::rings::Config &value) noexcept {
  ringBehavior_ = value;
  return *this;
}

AccessInfraStage &AccessInfraStage::deviceBehavior(
    const ::PhantomLedger::infra::synth::devices::Config &value) noexcept {
  deviceBehavior_ = value;
  return *this;
}

AccessInfraStage &AccessInfraStage::ipBehavior(
    const ::PhantomLedger::infra::synth::ips::Config &value) noexcept {
  ipBehavior_ = value;
  return *this;
}

AccessInfraStage &
AccessInfraStage::routingBehavior(RoutingBehavior value) noexcept {
  routingBehavior_ = value;
  return *this;
}

AccessInfraStage &AccessInfraStage::sharedInfra(SharedInfraUse value) noexcept {
  sharedInfra_ = value;
  return *this;
}

::PhantomLedger::time::Window AccessInfraStage::activeWindow(
    ::PhantomLedger::time::Window fallback) const noexcept {
  if (window_.days == 0) {
    return fallback;
  }

  return window_;
}

void AccessInfraStage::applySharedInfra(
    ::PhantomLedger::pipeline::Infra &out) const noexcept {
  out.ringInfra.ringDevice = out.devices.ringMap;
  out.ringInfra.ringIp = out.ips.ringMap;
  out.ringInfra.useSharedDeviceP = sharedInfra_.deviceP;
  out.ringInfra.useSharedIpP = sharedInfra_.ipP;
}

::PhantomLedger::pipeline::Infra
AccessInfraStage::build(::PhantomLedger::random::Rng &rng,
                        const ::PhantomLedger::pipeline::Entities &entities,
                        ::PhantomLedger::time::Window fallbackWindow) const {
  const auto window = activeWindow(fallbackWindow);
  ::PhantomLedger::pipeline::Infra out;

  out.ringPlans = ::PhantomLedger::infra::synth::rings::build(
      rng, window, entities.people.topology.rings, entities.people.topology,
      ringBehavior_);

  out.devices = ::PhantomLedger::infra::synth::devices::build(
      rng, window, entities.people.roster, out.ringPlans, deviceBehavior_);

  out.ips = ::PhantomLedger::infra::synth::ips::build(
      rng, window, entities.people.roster, out.ringPlans, ipBehavior_);

  out.router = ::PhantomLedger::infra::Router::build(
      routingBehavior_.switchP, buildOwnerMap(entities.accounts.registry),
      out.devices.byPerson, // copy — Router owns its pools
      out.ips.byPerson);

  applySharedInfra(out);

  return out;
}

::PhantomLedger::pipeline::Infra
build(::PhantomLedger::random::Rng &rng,
      const ::PhantomLedger::pipeline::Entities &entities,
      ::PhantomLedger::time::Window window,
      const ::PhantomLedger::infra::synth::rings::Config &ringBehavior,
      const ::PhantomLedger::infra::synth::devices::Config &deviceBehavior,
      const ::PhantomLedger::infra::synth::ips::Config &ipBehavior,
      RoutingBehavior routing, SharedInfraUse shared) {
  AccessInfraStage stage;
  return stage.window(window)
      .ringBehavior(ringBehavior)
      .deviceBehavior(deviceBehavior)
      .ipBehavior(ipBehavior)
      .routingBehavior(routing)
      .sharedInfra(shared)
      .build(rng, entities, window);
}

} // namespace PhantomLedger::pipeline::stages::infra
