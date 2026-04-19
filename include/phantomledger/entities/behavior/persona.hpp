#pragma once

#include "phantomledger/entities/behavior/archetype.hpp"
#include "phantomledger/entities/behavior/card.hpp"
#include "phantomledger/entities/behavior/cash.hpp"
#include "phantomledger/entities/behavior/payday.hpp"

namespace PhantomLedger::entities::behavior {

struct Persona {
  Archetype archetype;
  Cash cash;
  Card card;
  Payday payday;
};

} // namespace PhantomLedger::entities::behavior
