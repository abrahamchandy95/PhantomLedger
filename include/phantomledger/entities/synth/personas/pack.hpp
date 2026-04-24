#pragma once

#include "phantomledger/entities/behavior/behavior.hpp"

namespace PhantomLedger::entities::synth::personas {

struct Pack {
  entity::behavior::Assignment assignment;
  entity::behavior::Table table;
};

} // namespace PhantomLedger::entities::synth::personas
