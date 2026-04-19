#pragma once

#include "phantomledger/entities/identifier/key.hpp"

#include <cstdint>
#include <unordered_map>

namespace PhantomLedger::entities::accounts {

struct Lookup {
  std::unordered_map<identifier::Key, std::uint32_t> byId;
};

} // namespace PhantomLedger::entities::accounts
