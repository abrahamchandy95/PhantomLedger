#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/entities/synth/common/suffix.hpp"
#include "phantomledger/entities/synth/family/ids.hpp"

#include <optional>
#include <vector>

namespace PhantomLedger::entities::synth::family {

[[nodiscard]] inline bool usesExternal(entity::PersonId person,
                                       double externalP) {
  if (externalP <= 0.0) {
    return false;
  }
  if (externalP >= 1.0) {
    return true;
  }

  const auto value = common::suffix("external_bank", person);
  const double coin = static_cast<double>(value % 10000U) / 10000.0;
  return coin < externalP;
}

[[nodiscard]] inline std::optional<entity::Key>
pickMemberId(entity::PersonId person, const entity::account::Registry &registry,
             const entity::account::Ownership &ownership, double externalP) {
  if (person == entity::invalidPerson ||
      static_cast<std::size_t>(person) >= ownership.byPersonOffset.size()) {
    return std::nullopt;
  }

  const auto start = ownership.byPersonOffset[person - 1];
  const auto end = ownership.byPersonOffset[person];
  if (start == end) {
    return std::nullopt;
  }

  if (usesExternal(person, externalP)) {
    return id(person);
  }

  return registry.records[ownership.byPersonIndex[start]].id;
}

[[nodiscard]] inline std::vector<std::vector<entity::Key>>
plan(const entity::person::Roster &people,
     const entity::account::Ownership &ownership, double externalP) {
  std::vector<std::vector<entity::Key>> out(
      static_cast<std::size_t>(people.count) + 1);

  if (externalP <= 0.0) {
    return out;
  }

  for (entity::PersonId person = 1; person <= people.count; ++person) {
    const auto start = ownership.byPersonOffset[person - 1];
    const auto end = ownership.byPersonOffset[person];
    if (start == end) {
      continue;
    }

    if (usesExternal(person, externalP)) {
      out[person].push_back(id(person));
    }
  }

  return out;
}

} // namespace PhantomLedger::entities::synth::family
