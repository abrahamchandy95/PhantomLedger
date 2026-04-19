#pragma once

#include "phantomledger/entities/identifier/person.hpp"
#include "phantomledger/entities/people/flags.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::entities::synth::people {

inline void set(std::vector<std::uint8_t> &flags, identifier::PersonId person,
                entities::people::Flag flag) {
  flags[person - 1] |= entities::people::bit(flag);
}

} // namespace PhantomLedger::entities::synth::people
