#pragma once

#include "phantomledger/entities/accounts/record.hpp"
#include "phantomledger/entities/identifier/person.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::entities::accounts {

struct Registry {
  std::vector<Record> records;

  [[nodiscard]] bool hasOwner(std::uint32_t recIx) const noexcept {
    return records[recIx].owner != identifier::invalidPerson;
  }
};

} // namespace PhantomLedger::entities::accounts
