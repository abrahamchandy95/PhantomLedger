#pragma once

#include "phantomledger/personas/taxonomy.hpp"

namespace PhantomLedger::entities::behavior {

struct Archetype {
  personas::Kind kind = personas::Kind::salaried;
  personas::Timing timing = personas::Timing::consumer;
  double weight = 1.0;
};

} // namespace PhantomLedger::entities::behavior
