#pragma once

#include "phantomledger/entities/people/people.hpp"

namespace PhantomLedger::entities::synth::people {

struct Pack {
  entity::people::Roster roster;
  entity::people::Topology topology;
};

} // namespace PhantomLedger::entities::synth::people
