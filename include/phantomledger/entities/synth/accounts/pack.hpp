#pragma once

#include "phantomledger/entities/accounts/lookup.hpp"
#include "phantomledger/entities/accounts/ownership.hpp"
#include "phantomledger/entities/accounts/registry.hpp"

namespace PhantomLedger::entities::synth::accounts {

struct Pack {
  entities::accounts::Registry registry;
  entities::accounts::Ownership ownership;
  entities::accounts::Lookup lookup;
};

} // namespace PhantomLedger::entities::synth::accounts
