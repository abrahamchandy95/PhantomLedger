#pragma once

#include "phantomledger/entities/landlords/record.hpp"

#include <vector>

namespace PhantomLedger::entities::landlords {

struct Roster {
  std::vector<Record> records;
};

} // namespace PhantomLedger::entities::landlords
