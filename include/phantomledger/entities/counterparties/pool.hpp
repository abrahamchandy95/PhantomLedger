#pragma once

#include "phantomledger/entities/identifier/key.hpp"

#include <vector>

namespace PhantomLedger::entities::counterparties {

struct Pool {
  // Employers split by banking relationship.
  std::vector<entity::Key> employerInternalIds;
  std::vector<entity::Key> employerExternalIds;
  std::vector<entity::Key> employerIds; // combined

  // Clients split by banking relationship.
  std::vector<entity::Key> clientInternalIds;
  std::vector<entity::Key> clientExternalIds;
  std::vector<entity::Key> clientPayerIds; // combined

  // Fully external pools (platforms, processors, businesses,
  // brokerages bank at treasury/commercial banks, not retail).
  std::vector<entity::Key> platformIds;
  std::vector<entity::Key> processorIds;
  std::vector<entity::Key> ownerBusinessIds;
  std::vector<entity::Key> brokerageIds;

  // Aggregates for merge convenience.
  std::vector<entity::Key> allExternals;
  std::vector<entity::Key> allInternals;
};

} // namespace PhantomLedger::entities::counterparties
