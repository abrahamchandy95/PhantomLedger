#pragma once

#include "phantomledger/entities/merchants/record.hpp"

#include <vector>

namespace PhantomLedger::entities::merchants {

struct Catalog {
  std::vector<Record> records;
};

} // namespace PhantomLedger::entities::merchants
