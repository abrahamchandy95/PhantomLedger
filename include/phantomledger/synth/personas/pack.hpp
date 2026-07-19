#pragma once

#include "phantomledger/entities/parties/behaviors.hpp"

namespace PhantomLedger::synth::personas {

struct Pack {
  entity::behavior::Assignment assignment;
  entity::behavior::Table table;
};

} // namespace PhantomLedger::synth::personas
