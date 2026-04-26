#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/people.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::entities::synth::people {

inline void set(std::vector<std::uint8_t> &flags, entity::PersonId person,
                entity::person::Flag flag) {
  flags[person - 1] |= entity::person::bit(flag);
}

} // namespace PhantomLedger::entities::synth::people
