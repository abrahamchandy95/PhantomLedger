#pragma once

#include "phantomledger/taxonomies/personas/types.hpp"

namespace PhantomLedger::entities::behavior {

struct Archetype {
  personas::Type type = personas::Type::salaried;
  personas::Timing timing = personas::Timing::consumer;
  double weight = 1.0;
};

} // namespace PhantomLedger::entities::behavior
