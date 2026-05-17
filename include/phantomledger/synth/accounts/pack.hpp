#pragma once

#include "phantomledger/entities/accounts.hpp"

namespace PhantomLedger::synth::accounts {

struct Pack {
  entity::account::Registry registry;
  entity::account::Ownership ownership;
  entity::account::Lookup lookup;
};

} // namespace PhantomLedger::synth::accounts
