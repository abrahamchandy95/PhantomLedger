#pragma once

#include "phantomledger/entities/identifier/key.hpp"

#include <vector>

namespace PhantomLedger::entities::counterparties {

struct Pool {
  // Employers split by banking relationship.
  std::vector<identifier::Key> employerInternalIds;
  std::vector<identifier::Key> employerExternalIds;
  std::vector<identifier::Key> employerIds; // combined

  // Clients split by banking relationship.
  std::vector<identifier::Key> clientInternalIds;
  std::vector<identifier::Key> clientExternalIds;
  std::vector<identifier::Key> clientPayerIds; // combined

  // Fully external pools (platforms, processors, businesses,
  // brokerages bank at treasury/commercial banks, not retail).
  std::vector<identifier::Key> platformIds;
  std::vector<identifier::Key> processorIds;
  std::vector<identifier::Key> ownerBusinessIds;
  std::vector<identifier::Key> brokerageIds;

  // Aggregates for merge convenience.
  std::vector<identifier::Key> allExternals;
  std::vector<identifier::Key> allInternals;
};

} // namespace PhantomLedger::entities::counterparties
