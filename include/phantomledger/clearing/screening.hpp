#pragma once
/*
 * Balance screening utility.
 *
 * Advances a Ledger through a sorted sequence of base transactions
 * up to a given timestamp, so that upstream generators can check
 * affordability against a seeded balance state.
 */

#include "phantomledger/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace PhantomLedger::clearing {

/// Apply base transactions from baseTxns[startIdx..] whose timestamps
/// are before (or at, if inclusive) `untilTs`. Returns the new index
/// past the last consumed transaction.
///
/// If the ledger pointer is null, returns startIdx unchanged.
inline std::size_t
advanceBookThrough(Ledger *ledger,
                   std::span<const transactions::Transaction> baseTxns,
                   std::size_t startIdx, std::int64_t untilTs, bool inclusive) {
  if (ledger == nullptr) {
    return startIdx;
  }

  auto idx = startIdx;
  while (idx < baseTxns.size()) {
    const auto &txn = baseTxns[idx];

    if (inclusive) {
      if (txn.timestamp > untilTs) {
        break;
      }
    } else {
      if (txn.timestamp >= untilTs) {
        break;
      }
    }

    (void)ledger->transfer(txn.source, txn.target, txn.amount,
                           txn.session.channel);
    ++idx;
  }

  return idx;
}

} // namespace PhantomLedger::clearing
