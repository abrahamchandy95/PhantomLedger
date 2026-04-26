#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/synth/accounts/pack.hpp"

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace PhantomLedger::entities::synth::accounts {

inline void rebuild(Pack &pack, std::uint32_t people) {
  std::vector<std::uint32_t> counts(static_cast<std::size_t>(people), 0);

  for (const auto &record : pack.registry.records) {
    if (record.owner == entity::invalidPerson) {
      continue;
    }
    if (record.owner > people) {
      throw std::invalid_argument("account owner exceeds known people count");
    }
    ++counts[record.owner - 1];
  }

  pack.ownership.byPersonOffset.assign(static_cast<std::size_t>(people) + 1, 0);

  for (entity::PersonId person = 1; person <= people; ++person) {
    pack.ownership.byPersonOffset[person] =
        pack.ownership.byPersonOffset[person - 1] + counts[person - 1];
  }

  pack.ownership.byPersonIndex.assign(pack.ownership.byPersonOffset.back(), 0);

  auto next = pack.ownership.byPersonOffset;
  for (std::uint32_t recIx = 0;
       recIx < static_cast<std::uint32_t>(pack.registry.records.size());
       ++recIx) {
    const auto owner = pack.registry.records[recIx].owner;
    if (owner == entity::invalidPerson) {
      continue;
    }

    const auto slot = next[owner - 1]++;
    pack.ownership.byPersonIndex[slot] = recIx;
  }
}

} // namespace PhantomLedger::entities::synth::accounts
