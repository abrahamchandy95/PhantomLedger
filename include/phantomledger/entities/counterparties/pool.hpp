#pragma once

#include "phantomledger/entities/identifier/key.hpp"

#include <vector>

namespace PhantomLedger::entities::counterparties {

struct Pool {
  std::vector<identifier::Key> employerIds;
  std::vector<identifier::Key> clientPayerIds;
  std::vector<identifier::Key> platformIds;
  std::vector<identifier::Key> processorIds;
  std::vector<identifier::Key> ownerBusinessIds;
  std::vector<identifier::Key> brokerageIds;
  std::vector<identifier::Key> allExternals;
};

} // namespace PhantomLedger::entities::counterparties
