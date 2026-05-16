#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/people.hpp"

#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace PhantomLedger::transfers::fraud {

struct Plan {
  std::uint32_t ringId = 0;
  std::vector<entity::Key> fraudAccounts;
  std::vector<entity::Key> shellFraudAccounts;
  std::vector<entity::Key> muleAccounts;
  std::vector<entity::Key> victimAccounts;

  [[nodiscard]] std::vector<entity::Key> participantAccounts() const {
    std::vector<entity::Key> out;
    out.reserve(fraudAccounts.size() + muleAccounts.size());
    out.insert(out.end(), fraudAccounts.begin(), fraudAccounts.end());
    out.insert(out.end(), muleAccounts.begin(), muleAccounts.end());
    return out;
  }
};

namespace detail {

struct AccountView {
  const entity::account::Registry &registry;
  const entity::account::Ownership &ownership;
};

[[nodiscard]] inline entity::Key primaryKey(AccountView view,
                                            entity::PersonId person) {
  if (person == entity::invalidPerson ||
      static_cast<std::size_t>(person) >=
          view.ownership.byPersonOffset.size()) {
    throw std::runtime_error("Person has no active accounts: " +
                             std::to_string(person));
  }

  const auto start = view.ownership.byPersonOffset[person - 1];
  const auto end = view.ownership.byPersonOffset[person];
  if (start == end) {
    throw std::runtime_error("Person has no active accounts: " +
                             std::to_string(person));
  }

  return view.registry.records[view.ownership.primaryIndex(person)].id;
}

[[nodiscard]] inline std::vector<entity::Key>
collectKeys(AccountView view, std::span<const entity::PersonId> persons) {
  std::vector<entity::Key> out;
  out.reserve(persons.size());
  for (const auto p : persons) {
    out.push_back(primaryKey(view, p));
  }
  return out;
}

[[nodiscard]] inline std::vector<entity::Key>
collectKeysWithFlag(AccountView view, std::span<const entity::PersonId> persons,
                    entity::account::Flag flag) {
  std::vector<entity::Key> out;
  for (const auto p : persons) {
    const auto &rec = view.registry.records[view.ownership.primaryIndex(p)];
    if (entity::account::hasFlag(rec.flags, flag)) {
      out.push_back(rec.id);
    }
  }
  return out;
}

[[nodiscard]] inline std::span<const entity::PersonId>
sliceView(const std::vector<entity::PersonId> &store,
          entity::person::Slice slice) noexcept {
  return std::span<const entity::PersonId>(store.data() + slice.offset,
                                           slice.size);
}

} // namespace detail

[[nodiscard]] inline Plan
buildPlan(const entity::person::Ring &ring,
          const entity::person::Topology &topology,
          const entity::account::Registry &registry,
          const entity::account::Ownership &ownership) {
  const detail::AccountView view{registry, ownership};
  const auto fraudPersons = detail::sliceView(topology.fraudStore, ring.frauds);
  return Plan{
      .ringId = ring.id,
      .fraudAccounts = detail::collectKeys(view, fraudPersons),
      .shellFraudAccounts = detail::collectKeysWithFlag(
          view, fraudPersons, entity::account::Flag::shell),
      .muleAccounts = detail::collectKeys(
          view, detail::sliceView(topology.muleStore, ring.mules)),
      .victimAccounts = detail::collectKeys(
          view, detail::sliceView(topology.victimStore, ring.victims)),
  };
}

} // namespace PhantomLedger::transfers::fraud
