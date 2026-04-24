#pragma once

#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/entities/merchants/label.hpp"
#include "phantomledger/taxonomies/merchants/types.hpp"

namespace PhantomLedger::entities::merchants {

struct Record {
  Label label;
  entity::Key counterpartyId;
  ::PhantomLedger::merchants::Category category;
  double weight = 0.0;
};

} // namespace PhantomLedger::entities::merchants
