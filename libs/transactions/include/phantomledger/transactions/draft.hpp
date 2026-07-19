#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"

#include <cstdint>

namespace PhantomLedger::transactions {

struct Draft {
  entity::Key source;
  entity::Key destination;
  double amount = 0.0;
  std::int64_t timestamp = 0;

  std::uint8_t isFraud = 0;
  std::int32_t ringId = -1;
  channels::Tag channel = channels::none;

  std::int32_t chainId = -1;
};

} // namespace PhantomLedger::transactions
