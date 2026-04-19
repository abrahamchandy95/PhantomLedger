#pragma once

#include "phantomledger/personas/taxonomy.hpp"

#include <vector>

namespace PhantomLedger::entities::behavior {

struct Assignment {
  std::vector<personas::Kind> byPerson; // index = personId - 1
};

} // namespace PhantomLedger::entities::behavior
