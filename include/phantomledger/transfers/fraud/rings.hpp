#pragma once

#include "phantomledger/entities/holdings/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/parties/people.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/personas/timeline.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
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

  // H3 part 3c-ii (authority U-8 addendum): the ring's ALIVE HORIZON —
  // the minimum death epoch over its ACTIVE participants (fraud
  // persons + mules; victims are exempt: fraud against deceased
  // persons' accounts is a real typology). The injector clamps the
  // ring's typology bursts and camouflage window to this horizon minus
  // a schedule guard, so rings never recruit the dead. int64 max when
  // no timeline carrier is available (the intervals stand down).
  std::int64_t participantsAliveEndEpoch =
      std::numeric_limits<std::int64_t>::max();

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

// The minimum death epoch across the ring's ACTIVE participants (see
// Plan::participantsAliveEndEpoch). Persons outside the carrier keep
// the max() sentinel.
[[nodiscard]] inline std::int64_t participantsAliveEnd(
    std::span<const synth::personas::timeline::Timeline> timelines,
    std::span<const entity::PersonId> fraudPersons,
    std::span<const entity::PersonId> mulePersons) noexcept {
  auto horizon = std::numeric_limits<std::int64_t>::max();
  if (timelines.empty()) {
    return horizon;
  }
  const auto fold = [&](std::span<const entity::PersonId> persons) {
    for (const auto p : persons) {
      if (p == 0 || static_cast<std::size_t>(p) > timelines.size()) {
        continue;
      }
      horizon = std::min(horizon,
                         time::toEpochSeconds(timelines[p - 1].death));
    }
  };
  fold(fraudPersons);
  fold(mulePersons);
  return horizon;
}

} // namespace detail

[[nodiscard]] inline Plan
buildPlan(const entity::person::Ring &ring,
          const entity::person::Topology &topology,
          const entity::account::Registry &registry,
          const entity::account::Ownership &ownership,
          std::span<const synth::personas::timeline::Timeline> timelines = {}) {
  const detail::AccountView view{registry, ownership};
  const auto fraudPersons = detail::sliceView(topology.fraudStore, ring.frauds);
  const auto mulePersons = detail::sliceView(topology.muleStore, ring.mules);
  return Plan{
      .ringId = ring.id,
      .fraudAccounts = detail::collectKeys(view, fraudPersons),
      .shellFraudAccounts = detail::collectKeysWithFlag(
          view, fraudPersons, entity::account::Flag::shell),
      .muleAccounts = detail::collectKeys(view, mulePersons),
      .victimAccounts = detail::collectKeys(
          view, detail::sliceView(topology.victimStore, ring.victims)),
      .participantsAliveEndEpoch =
          detail::participantsAliveEnd(timelines, fraudPersons, mulePersons),
  };
}

} // namespace PhantomLedger::transfers::fraud
