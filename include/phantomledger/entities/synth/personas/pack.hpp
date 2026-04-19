#pragma once

#include "phantomledger/entities/behavior/assignment.hpp"
#include "phantomledger/entities/behavior/table.hpp"

namespace PhantomLedger::entities::synth::personas {

struct Pack {
  entities::behavior::Assignment assignment;
  entities::behavior::Table table;
};

} // namespace PhantomLedger::entities::synth::personas
