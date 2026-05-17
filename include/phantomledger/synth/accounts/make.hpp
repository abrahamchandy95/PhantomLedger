#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/synth/accounts/counts.hpp"
#include "phantomledger/synth/accounts/pack.hpp"

#include <cstdint>
#include <numeric>

namespace PhantomLedger::synth::accounts {

using identifiers::Bank;
using identifiers::Role;

namespace detail {

inline constexpr double kShellRate = 0.35;

inline void applyPersonFlags(entity::account::Record &primary,
                             const entity::person::Roster &people,
                             entity::PersonId person, random::Rng &rng) {
  using entity::account::Flag;

  if (people.has(person, entity::person::Flag::fraud)) {
    primary.flags |= entity::account::bit(Flag::fraud);
    if (rng.coin(kShellRate)) {
      primary.flags |= entity::account::bit(Flag::shell);
    }
  }
  if (people.has(person, entity::person::Flag::mule)) {
    primary.flags |= entity::account::bit(Flag::mule);
  }
  if (people.has(person, entity::person::Flag::victim)) {
    primary.flags |= entity::account::bit(Flag::victim);
  }
}

} // namespace detail

[[nodiscard]] inline Pack makePack(random::Rng &rng,
                                   const entity::person::Roster &people,
                                   int maxPerPerson = 3) {
  const auto perPerson =
      counts(rng, static_cast<int>(people.count), maxPerPerson);
  const auto total = static_cast<std::uint32_t>(
      std::accumulate(perPerson.begin(), perPerson.end(), 0));

  Pack out;
  out.registry.records.reserve(total);
  out.ownership.byPersonOffset.resize(static_cast<std::size_t>(people.count) +
                                      1);
  out.ownership.byPersonIndex.reserve(total);
  out.lookup.byId.reserve(total);

  std::uint64_t serial = 1;
  std::uint32_t offset = 0;

  for (entity::PersonId person = 1; person <= people.count; ++person) {
    out.ownership.byPersonOffset[person - 1] = offset;

    for (int i = 0; i < perPerson[person - 1]; ++i) {
      const auto recIx =
          static_cast<std::uint32_t>(out.registry.records.size());
      const auto id = entity::makeKey(Role::account, Bank::internal, serial++);

      out.registry.records.push_back(entity::account::Record{
          .id = id,
          .owner = person,
          .flags = 0,
      });

      out.ownership.byPersonIndex.push_back(recIx);
      out.lookup.byId.emplace(id, recIx);
      ++offset;
    }
  }

  out.ownership.byPersonOffset[people.count] = offset;

  for (entity::PersonId person = 1; person <= people.count; ++person) {
    auto &primary = out.registry.records[out.ownership.primaryIndex(person)];
    detail::applyPersonFlags(primary, people, person, rng);
  }

  return out;
}

} // namespace PhantomLedger::synth::accounts
