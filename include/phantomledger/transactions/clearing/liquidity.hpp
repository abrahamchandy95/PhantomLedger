#pragma once
/*
 * Liquidity events emitted by the ledger during authoritative replay.
 */

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"

#include <cstdint>
#include <functional>

namespace PhantomLedger::clearing {

struct LiquidityEvent {
  channels::Tag channel;
  entity::Key payerKey;
  double amount = 0.0;
  std::int64_t timestamp = 0;
};

using LiquiditySink = std::function<void(const LiquidityEvent &)>;

} // namespace PhantomLedger::clearing
