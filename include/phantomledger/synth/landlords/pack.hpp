#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/landlords.hpp"

#include <vector>

namespace PhantomLedger::synth::landlords {

struct Pack {
  entity::landlord::Roster roster;
  entity::landlord::Index index;

  std::vector<entity::Key> internals;

  std::vector<entity::Key> externals;
};

} // namespace PhantomLedger::synth::landlords
