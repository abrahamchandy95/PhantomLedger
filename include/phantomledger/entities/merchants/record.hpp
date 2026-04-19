#pragma once

#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/entities/merchants/label.hpp"

#include <cstdint>

namespace PhantomLedger::entities::merchants {

struct Record {
  Label label;
  identifier::Key counterpartyId;
  std::uint16_t categoryIndex = 0;
  double weight = 0.0;
};

} // namespace PhantomLedger::entities::merchants
