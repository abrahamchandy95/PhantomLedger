#include "phantomledger/pipeline/stages/infra.hpp"

#include <unordered_map>
#include <vector>

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

::PhantomLedger::pipeline::Infra
build(::PhantomLedger::random::Rng &rng,
      const ::PhantomLedger::pipeline::Entities &entities,
      const InfraSynthesis &infra) {
  ::PhantomLedger::pipeline::Infra out;

  out.ringPlans = ::PhantomLedger::infra::synth::rings::build(
      rng, infra.window, entities.people.topology.rings,
      entities.people.topology, infra.ringBehavior);

  out.devices = ::PhantomLedger::infra::synth::devices::build(
      rng, infra.window, entities.people.roster, out.ringPlans,
      infra.deviceBehavior);

  out.ips = ::PhantomLedger::infra::synth::ips::build(
      rng, infra.window, entities.people.roster, out.ringPlans,
      infra.ipBehavior);

  out.router = ::PhantomLedger::infra::Router::build(
      infra.routerSwitchP, buildOwnerMap(entities.accounts.registry),
      out.devices.byPerson, // copy — Router owns its pools
      out.ips.byPerson);

  out.ringInfra.ringDevice = out.devices.ringMap;
  out.ringInfra.ringIp = out.ips.ringMap;
  out.ringInfra.useSharedDeviceP = infra.sharedDeviceUseP;
  out.ringInfra.useSharedIpP = infra.sharedIpUseP;

  return out;
}

} // namespace PhantomLedger::pipeline::stages::infra
