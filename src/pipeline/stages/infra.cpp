#include "phantomledger/pipeline/stages/infra.hpp"

#include "phantomledger/primitives/validate/checks.hpp"

#include <ranges>
#include <unordered_map>

namespace PhantomLedger::pipeline::stages::infra {

namespace {

namespace entities = ::PhantomLedger::entities;
namespace entity = ::PhantomLedger::entity;
namespace primitives = ::PhantomLedger::primitives;
namespace random = ::PhantomLedger::random;
namespace synth = ::PhantomLedger::synth;
namespace time_ns = ::PhantomLedger::time;

[[nodiscard]] std::unordered_map<entity::Key, entity::PersonId>
buildOwnerMap(const entity::account::Registry &registry) {
  std::unordered_map<entity::Key, entity::PersonId> out;
  out.reserve(registry.records.size());

  auto is_valid_owner = [](const auto &record) {
    return record.owner != entity::invalidPerson;
  };

  for (const auto &record :
       registry.records | std::views::filter(is_valid_owner)) {
    out.emplace(record.id, record.owner);
  }

  return out;
}

} // namespace

AccessInfraStage &AccessInfraStage::window(time_ns::Window value) noexcept {
  window_ = value;
  return *this;
}

AccessInfraStage &
AccessInfraStage::ringAccess(synth::infra::rings::AccessRules value) noexcept {
  ringAccess_ = value;
  return *this;
}

AccessInfraStage &AccessInfraStage::deviceAssignment(
    synth::infra::devices::AssignmentRules value) noexcept {
  deviceAssignment_ = value;
  return *this;
}

AccessInfraStage &AccessInfraStage::ipAssignment(
    synth::infra::ips::AssignmentRules value) noexcept {
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

time_ns::Window
AccessInfraStage::activeWindow(time_ns::Window fallback) const noexcept {
  return window_.value_or(fallback);
}

AccessInfraStage::RingPlans AccessInfraStage::buildRingPlans(
    random::Rng &rng, time_ns::Window window,
    const entities::synth::people::Pack &people) const {
  return ringAccess_.build(rng, window, people.topology.rings, people.topology);
}

synth::infra::devices::Output
AccessInfraStage::buildDevices(random::Rng &rng, time_ns::Window window,
                               const entity::person::Roster &people,
                               const RingPlans &ringPlans) const {
  return deviceAssignment_.build(rng, window, people, ringPlans);
}

synth::infra::ips::Output
AccessInfraStage::buildIps(random::Rng &rng, time_ns::Window window,
                           const entity::person::Roster &people,
                           const RingPlans &ringPlans) const {
  return ipAssignment_.build(rng, window, people, ringPlans);
}

::PhantomLedger::infra::Router
AccessInfraStage::buildRouter(const entity::account::Registry &accounts,
                              const synth::infra::devices::Output &devices,
                              const synth::infra::ips::Output &ips) const {
  return ::PhantomLedger::infra::Router::build(
      routerRules_, buildOwnerMap(accounts), devices.byPerson, ips.byPerson);
}

::PhantomLedger::infra::SharedInfra
AccessInfraStage::buildSharedInfra(const synth::infra::devices::Output &devices,
                                   const synth::infra::ips::Output &ips) const {
  ::PhantomLedger::infra::SharedInfra out;
  out.ringDevice = devices.ringMap;
  out.ringIp = ips.ringMap;
  out.apply(sharedInfra_);
  return out;
}

pipe::Infra AccessInfraStage::build(random::Rng &rng,
                                    const pipe::Entities &entities,
                                    time_ns::Window fallbackWindow) const {

  primitives::validate::Report report;
  ringAccess_.validate(report);
  deviceAssignment_.validate(report);
  ipAssignment_.validate(report);
  routerRules_.validate(report);
  sharedInfra_.validate(report);
  report.throwIfFailed();

  const auto window = activeWindow(fallbackWindow);

  pipe::Infra out;
  out.ringPlans = buildRingPlans(rng, window, entities.people);
  out.devices =
      buildDevices(rng, window, entities.people.roster, out.ringPlans);
  out.ips = buildIps(rng, window, entities.people.roster, out.ringPlans);
  out.router = buildRouter(entities.accounts.registry, out.devices, out.ips);
  out.ringInfra = buildSharedInfra(out.devices, out.ips);

  return out;
}

pipe::Infra build(random::Rng &rng, const pipe::Entities &entities,
                  time_ns::Window window) {
  return AccessInfraStage{}.build(rng, entities, window);
}

} // namespace PhantomLedger::pipeline::stages::infra
