#pragma once

#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/entities/people/people.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::entities::synth::people {

inline void set(std::vector<std::uint8_t> &flags, entity::PersonId person,
                entity::people::Flag flag) {
  flags[person - 1] |= entity::people::bit(flag);
}

} // namespace PhantomLedger::entities::synth::people
