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
      const ::PhantomLedger::pipeline::Entities &entities, const Inputs &in) {
  ::PhantomLedger::pipeline::Infra out;

  out.ringPlans = ::PhantomLedger::infra::synth::rings::build(
      rng, in.window, entities.people.topology.rings, entities.people.topology,
      in.ringsCfg);

  out.devices = ::PhantomLedger::infra::synth::devices::build(
      rng, in.window, entities.people.roster, out.ringPlans, in.devicesCfg);

  out.ips = ::PhantomLedger::infra::synth::ips::build(
      rng, in.window, entities.people.roster, out.ringPlans, in.ipsCfg);

  out.router = ::PhantomLedger::infra::Router::build(
      in.switchP, buildOwnerMap(entities.accounts.registry),
      out.devices.byPerson, // copy — Router owns its pools
      out.ips.byPerson);

  out.ringInfra.ringDevice = out.devices.ringMap;
  out.ringInfra.ringIp = out.ips.ringMap;
  out.ringInfra.useSharedDeviceP = in.useSharedDeviceP;
  out.ringInfra.useSharedIpP = in.useSharedIpP;

  return out;
}

} // namespace PhantomLedger::pipeline::stages::infra
