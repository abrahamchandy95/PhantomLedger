#pragma once

#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/entities/identifier/person.hpp"

#include <cstdint>

namespace PhantomLedger::entities::accounts {

struct Record {
  identifier::Key id;
  identifier::PersonId owner = identifier::invalidPerson;
  std::uint8_t flags = 0;
};

} // namespace PhantomLedger::entities::accounts
