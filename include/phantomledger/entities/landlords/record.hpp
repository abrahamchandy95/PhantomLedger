#pragma once

#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/entities/landlords/class.hpp"

namespace PhantomLedger::entities::landlords {

struct Record {
  identifier::Key accountId;
  Class type = Class::individual;
};

} // namespace PhantomLedger::entities::landlords
