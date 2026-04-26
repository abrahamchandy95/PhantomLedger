#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/behaviors.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/entities/synth/inflow/ids.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <vector>

namespace PhantomLedger::entities::synth::inflow {

[[nodiscard]] inline std::vector<std::vector<entity::Key>>
planInflowIds(const entity::person::Roster &people,
              const entity::behavior::Assignment &assignment,
              const entity::account::Ownership &ownership) {
  std::vector<std::vector<entity::Key>> out(
      static_cast<std::size_t>(people.count) + 1);

  for (entity::PersonId person = 1; person <= people.count; ++person) {
    const auto start = ownership.byPersonOffset[person - 1];
    const auto end = ownership.byPersonOffset[person];
    if (start == end) {
      continue;
    }

    const auto kind = assignment.byPerson[person - 1];
    if (kind == personas::Type::freelancer ||
        kind == personas::Type::smallBusiness) {
      out[person].push_back(businessId(person));
    } else if (kind == personas::Type::highNetWorth) {
      out[person].push_back(brokerageId(person));
    }
  }

  return out;
}

} // namespace PhantomLedger::entities::synth::inflow
