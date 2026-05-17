#pragma once

#include "phantomledger/entities/people.hpp"

namespace PhantomLedger::synth::people {

struct Pack {
  entity::person::Roster roster;
  entity::person::Topology topology;
};

} // namespace PhantomLedger::synth::people
