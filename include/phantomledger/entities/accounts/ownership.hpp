#pragma once

#include "phantomledger/entities/identifier/person.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::entities::accounts {

struct Ownership {
  std::vector<std::uint32_t> byPersonOffset; // size = people.count + 1
  std::vector<std::uint32_t> byPersonIndex;  // account record indices

  [[nodiscard]] std::uint32_t
  primaryIndex(identifier::PersonId person) const noexcept {
    return byPersonIndex[byPersonOffset[person - 1]];
  }
};

} // namespace PhantomLedger::entities::accounts
