#pragma once

#include "phantomledger/entities/identifier/person.hpp"
#include "phantomledger/entities/people/flags.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::entities::people {

struct Roster {
  std::uint32_t count = 0;
  std::vector<std::uint8_t> flags; // index = personId - 1

  [[nodiscard]] bool has(identifier::PersonId person,
                         Flag flag) const noexcept {
    return person > 0 && person <= count &&
           (flags[person - 1] & bit(flag)) != 0;
  }
};

} // namespace PhantomLedger::entities::people
