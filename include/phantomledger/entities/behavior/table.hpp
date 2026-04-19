#pragma once

#include "phantomledger/entities/behavior/persona.hpp"

#include <vector>

namespace PhantomLedger::entities::behavior {

struct Table {
  std::vector<Persona> byPerson; // index = personId - 1
};

} // namespace PhantomLedger::entities::behavior
