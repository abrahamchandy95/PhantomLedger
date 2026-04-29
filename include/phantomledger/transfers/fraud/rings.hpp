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

[[nodiscard]] inline entity::Key
primaryKey(const entity::account::Registry &registry,
           const entity::account::Ownership &ownership,
           entity::PersonId person) {
  if (person == entity::invalidPerson ||
      static_cast<std::size_t>(person) >= ownership.byPersonOffset.size()) {
    throw std::runtime_error("Person has no active accounts: " +
                             std::to_string(person));
  }

  const auto start = ownership.byPersonOffset[person - 1];
  const auto end = ownership.byPersonOffset[person];
  if (start == end) {
    throw std::runtime_error("Person has no active accounts: " +
                             std::to_string(person));
  }

  return registry.records[ownership.primaryIndex(person)].id;
}

[[nodiscard]] inline std::vector<entity::Key>
collectKeys(const entity::account::Registry &registry,
            const entity::account::Ownership &ownership,
            std::span<const entity::PersonId> persons) {
  std::vector<entity::Key> out;
  out.reserve(persons.size());
  for (const auto p : persons) {
    out.push_back(primaryKey(registry, ownership, p));
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
  return Plan{
      .ringId = ring.id,
      .fraudAccounts = detail::collectKeys(
          registry, ownership,
          detail::sliceView(topology.fraudStore, ring.frauds)),
      .muleAccounts = detail::collectKeys(
          registry, ownership,
          detail::sliceView(topology.muleStore, ring.mules)),
      .victimAccounts = detail::collectKeys(
          registry, ownership,
          detail::sliceView(topology.victimStore, ring.victims)),
  };
}

} // namespace PhantomLedger::transfers::fraud
