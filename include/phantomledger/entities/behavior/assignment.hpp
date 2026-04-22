#pragma once

#include "phantomledger/taxonomies/personas/types.hpp"

#include <vector>

namespace PhantomLedger::entities::behavior {

struct Assignment {
  std::vector<personas::Type> byPerson; // index = personId - 1
};

} // namespace PhantomLedger::entities::behavior
