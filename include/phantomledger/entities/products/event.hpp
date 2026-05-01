#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/products/types.hpp"

#include <cstdint>

namespace PhantomLedger::entity::product {

using Direction = ::PhantomLedger::products::Direction;
using ProductType = ::PhantomLedger::products::ProductType;

struct ObligationEvent {
  entity::PersonId personId{};
  Direction direction = Direction::outflow;

  /// External counterparty (lender, carrier, IRS, etc.).
  entity::Key counterpartyAcct{};

  double amount = 0.0;
  time::TimePoint timestamp{};
  channels::Tag channel = channels::none;

  ProductType productType = ProductType::unknown;
  std::uint32_t productId = 0;
};

} // namespace PhantomLedger::entity::product
