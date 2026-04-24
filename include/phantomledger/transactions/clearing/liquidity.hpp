#pragma once
/*
 * Liquidity events emitted by the ledger during authoritative replay.
 *
 * When a transfer causes an account to cross into negative cash,
 * Ledger::transferAt() emits an OVERDRAFT_FEE event (for COURTESY and
 * LINKED protection) that debits the account and records the fee. For
 * LOC-protected accounts, dollar-seconds are accumulated on every
 * transfer and periodically turned into a LOC_INTEREST event by
 * Ledger::accrueLocInterestThrough().
 *
 * The consumer of the sink is responsible for turning these events
 * into full Transaction records (with source = payerKey, target = the
 * bank servicing account, plus device/IP attribution via the factory).
 * Keeping the ledger out of Transaction construction avoids a circular
 * dependency on the transaction factory.
 *
 * The authoritative replay pass runs with emitLiquidity=true. The
 * post-fraud replay pass runs with emitLiquidity=false so the fees
 * don't double-emit.
 */

#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"

#include <cstdint>
#include <functional>

namespace PhantomLedger::clearing {

struct LiquidityEvent {
  channels::Tag channel; // overdraftFee or locInterest
  entity::Key payerKey;  // the account being debited
  double amount = 0.0;
  std::int64_t timestamp = 0;
};

using LiquiditySink = std::function<void(const LiquidityEvent &)>;

} // namespace PhantomLedger::clearing
