#pragma once

#include "phantomledger/entities/identifier/person.hpp"
#include "phantomledger/entities/people/ring.hpp"

#include <vector>

namespace PhantomLedger::entities::people {

struct Topology {
  std::vector<identifier::PersonId> memberStore;
  std::vector<identifier::PersonId> fraudStore;
  std::vector<identifier::PersonId> muleStore;
  std::vector<identifier::PersonId> victimStore;
  std::vector<Ring> rings;
};

} // namespace PhantomLedger::entities::people
