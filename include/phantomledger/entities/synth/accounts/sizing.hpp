#pragma once

#include <cstdint>

namespace PhantomLedger::entities::synth::accounts {

struct Sizing {
  std::int32_t maxAccountsPerPerson = 3;
};

} // namespace PhantomLedger::entities::synth::accounts
