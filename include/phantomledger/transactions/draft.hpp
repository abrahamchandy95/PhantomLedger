#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"

#include <cstdint>

namespace PhantomLedger::transactions {

/// Raw transfer specification before infrastructure attribution.
///
/// The factory converts this into a full `Transaction` by attaching
/// device/IP from the routing layer.
struct Draft {
  entity::Key source;
  entity::Key destination;
  double amount = 0.0;
  std::int64_t timestamp = 0;

  std::uint8_t isFraud = 0;
  std::int32_t ringId = -1;
  channels::Tag channel = channels::none;
};

} // namespace PhantomLedger::transactions
