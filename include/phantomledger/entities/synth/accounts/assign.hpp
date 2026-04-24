#pragma once

#include "phantomledger/entities/accounts/account.hpp"
#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/entities/synth/accounts/owners.hpp"

#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace PhantomLedger::entities::synth::accounts {

inline void addAccounts(Pack &pack, std::span<const entity::Key> ids,
                        bool external = false) {
  pack.registry.records.reserve(pack.registry.records.size() + ids.size());
  pack.lookup.byId.reserve(pack.lookup.byId.size() + ids.size());

  const auto extra = external
                         ? entity::account::bit(entity::account::Flag::external)
                         : static_cast<std::uint8_t>(0);

  for (const auto id : ids) {
    if (pack.lookup.byId.contains(id)) {
      if (external) {
        pack.registry.records[pack.lookup.byId.at(id)].flags |= extra;
      }
      continue;
    }

    const auto recIx = static_cast<std::uint32_t>(pack.registry.records.size());
    pack.registry.records.push_back(entity::account::Record{
        .id = id,
        .owner = entity::invalidPerson,
        .flags = extra,
    });
    pack.lookup.byId.emplace(id, recIx);
  }
}

inline void
assignOwners(Pack &pack,
             std::span<const std::vector<entity::Key>> ownedByPerson,
             bool external = false) {
  if (ownedByPerson.empty()) {
    return;
  }
  if (ownedByPerson.size() != pack.ownership.byPersonOffset.size()) {
    throw std::invalid_argument(
        "own: ownedByPerson size must match people.count + 1");
  }

  std::size_t incoming = 0;
  for (std::size_t i = 1; i < ownedByPerson.size(); ++i) {
    incoming += ownedByPerson[i].size();
  }

  pack.registry.records.reserve(pack.registry.records.size() + incoming);
  pack.lookup.byId.reserve(pack.lookup.byId.size() + incoming);

  const auto extra = external
                         ? entity::account::bit(entity::account::Flag::external)
                         : static_cast<std::uint8_t>(0);
  const auto people = static_cast<entity::PersonId>(ownedByPerson.size() - 1);

  for (entity::PersonId person = 1; person <= people; ++person) {
    for (const auto id : ownedByPerson[person]) {
      const auto found = pack.lookup.byId.find(id);

      if (found == pack.lookup.byId.end()) {
        const auto recIx =
            static_cast<std::uint32_t>(pack.registry.records.size());
        pack.registry.records.push_back(entity::account::Record{
            .id = id,
            .owner = person,
            .flags = extra,
        });
        pack.lookup.byId.emplace(id, recIx);
        continue;
      }

      auto &record = pack.registry.records[found->second];
      if (record.owner != entity::invalidPerson && record.owner != person) {
        throw std::invalid_argument(
            "own: account already belongs to a different owner");
      }

      record.owner = person;
      record.flags |= extra;
    }
  }

  rebuild(pack, people);
}

} // namespace PhantomLedger::entities::synth::accounts
