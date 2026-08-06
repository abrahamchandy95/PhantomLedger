#include "phantomledger/pipeline/stages/infra.hpp"

#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

#include <ranges>
#include <unordered_map>

namespace PhantomLedger::pipeline::stages::infra {

namespace {

namespace pipe = ::PhantomLedger::pipeline;
namespace entity = ::PhantomLedger::entity;
namespace primitives = ::PhantomLedger::primitives;
namespace random = ::PhantomLedger::random;
namespace synth = ::PhantomLedger::synth;
namespace time_ns = ::PhantomLedger::time;

[[nodiscard]] std::unordered_map<entity::Key, entity::PersonId>
buildOwnerMap(const entity::account::Registry &registry,
              const entity::card::Registry &cards) {
  std::unordered_map<entity::Key, entity::PersonId> out;
  out.reserve(registry.records.size() + cards.records.size());

  auto is_valid_owner = [](const auto &record) {
    return record.owner != entity::invalidPerson;
  };

  for (const auto &record :
       registry.records | std::views::filter(is_valid_owner)) {
    out.emplace(record.id, record.owner);
  }

  // Credit-card liabilities live in the account registry for clearing but
  // keep their ownership in the card registry so ordinary cash-account
  // ownership slices do not accidentally treat a liability as a deposit
  // account. The access router still needs that owner: without this merge,
  // every legitimate credit-card purchase had an empty device/IP session.
  for (const auto &card : cards.records) {
    if (card.owner != entity::invalidPerson) {
      out.insert_or_assign(card.key, card.owner);
    }
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

AccessInfraStage &AccessInfraStage::attackerAssignment(
    synth::infra::attackers::AssignmentRules value) noexcept {
  attackerAssignment_ = value;
  return *this;
}

AccessInfraStage &AccessInfraStage::sharedInfra(
    ::PhantomLedger::infra::SharedInfraRules value) noexcept {
  sharedInfra_ = value;
  return *this;
}

::PhantomLedger::infra::SharedInfra
AccessInfraStage::buildSharedInfra(const synth::infra::devices::Output &devices,
                                   const synth::infra::ips::Output &ips) const {
  ::PhantomLedger::infra::SharedInfra out{
      .ringDevice = devices.ringMap,
      .ringIp = ips.ringMap,
  };
  out.apply(sharedInfra_);
  return out;
}

pipe::Infra AccessInfraStage::build(random::Rng &rng,
                                    const pipe::People &people,
                                    const pipe::Holdings &holdings,
                                    time_ns::Window fallbackWindow,
                                    std::uint64_t runSeed) const {

  primitives::validate::Report report;
  ringAccess_.validate(report);
  deviceAssignment_.validate(report);
  ipAssignment_.validate(report);
  routerRules_.validate(report);
  sharedInfra_.validate(report);
  attackerAssignment_.validate(report);
  report.throwIfFailed();

  const auto window = window_.value_or(fallbackWindow);
  const auto &peoplePack = people.roster;

  pipe::Infra out;
  out.ringPlans = ringAccess_.build(rng, window, peoplePack.topology.rings,
                                    peoplePack.topology);
  out.devices =
      deviceAssignment_.build(rng, window, peoplePack.roster, out.ringPlans);
  out.ips = ipAssignment_.build(rng, window, peoplePack.roster, out.ringPlans);
  // The tenure arrays are what make session attribution point-in-time
  // honest: the router may only hand back an endpoint whose interval
  // contains the row's timestamp.
  out.router = ::PhantomLedger::infra::Router::build(
      routerRules_,
      buildOwnerMap(holdings.accounts.registry, holdings.creditCards),
      out.devices.byPerson, out.ips.byPerson, out.devices.tenureByPerson,
      out.ips.tenureByPerson);
  out.ringInfra = buildSharedInfra(out.devices, out.ips);

  /* The exogenous fraud-infrastructure pool, on its OWN keyed lane off the
   * run seed. See the header for why it is not taken off `rng`. */
  {
    const random::RngFactory laneFactory{runSeed};
    auto attackerRng = laneFactory.rng({"infra", "attackers"});
    out.attackers =
        attackerAssignment_.build(attackerRng, window, peoplePack.roster.count);
  }

  return out;
}

} // namespace PhantomLedger::pipeline::stages::infra
