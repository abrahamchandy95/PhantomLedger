#pragma once

#include "phantomledger/entities/accounts/flags.hpp"
#include "phantomledger/entities/accounts/record.hpp"
#include "phantomledger/entities/identifier/make.hpp"
#include "phantomledger/entities/people/flags.hpp"
#include "phantomledger/entities/people/roster.hpp"
#include "phantomledger/entities/synth/accounts/counts.hpp"
#include "phantomledger/entities/synth/accounts/pack.hpp"

#include <cstdint>
#include <numeric>

namespace PhantomLedger::entities::synth::accounts {

using taxonomies::identifiers::Bank;
using taxonomies::identifiers::Role;

[[nodiscard]] inline Pack
makePack(random::Rng &rng, const people::Roster &people, int maxPerPerson = 3) {
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

  for (identifier::PersonId person = 1; person <= people.count; ++person) {
    out.ownership.byPersonOffset[person - 1] = offset;

    for (int i = 0; i < perPerson[person - 1]; ++i) {
      const auto recIx =
          static_cast<std::uint32_t>(out.registry.records.size());
      const auto id = identifier::make(Role::account, Bank::internal, serial++);

      out.registry.records.push_back(entities::accounts::Record{
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

  for (identifier::PersonId person = 1; person <= people.count; ++person) {
    auto &primary = out.registry.records[out.ownership.primaryIndex(person)];

    if (people.has(person, people::Flag::fraud)) {
      primary.flags |= entities::accounts::bit(entities::accounts::Flag::fraud);
    }
    if (people.has(person, people::Flag::mule)) {
      primary.flags |= entities::accounts::bit(entities::accounts::Flag::mule);
    }
    if (people.has(person, people::Flag::victim)) {
      primary.flags |=
          entities::accounts::bit(entities::accounts::Flag::victim);
    }
  }

  return out;
}

} // namespace PhantomLedger::entities::synth::accounts
