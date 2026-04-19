#pragma once

#include "phantomledger/entities/people/roster.hpp"
#include "phantomledger/entities/people/topology.hpp"

namespace PhantomLedger::entities::synth::people {

struct Pack {
  entities::people::Roster roster;
  entities::people::Topology topology;
};

} // namespace PhantomLedger::entities::synth::people
