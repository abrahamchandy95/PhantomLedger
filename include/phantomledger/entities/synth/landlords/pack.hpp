#pragma once

#include "phantomledger/entities/landlords/index.hpp"
#include "phantomledger/entities/landlords/roster.hpp"

namespace PhantomLedger::entities::synth::landlords {

struct Pack {
  entities::landlords::Roster roster;
  entities::landlords::Index index;
};

} // namespace PhantomLedger::entities::synth::landlords
