#pragma once

#include "ledger.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace PhantomLedger::clearing {

struct TimeBound {
  std::int64_t until = 0;
  bool inclusive = false;

  /// True if `ts` falls within this bound
  [[nodiscard]] constexpr bool includes(std::int64_t ts) const noexcept {
    return inclusive ? ts <= until : ts < until;
  }
};

inline std::size_t
advanceBookThrough(Ledger *ledger,
                   std::span<const transactions::Transaction> baseTxns,
                   std::size_t startIdx, TimeBound bound) {
  if (ledger == nullptr) {
    return startIdx;
  }

  auto idx = startIdx;
  while (idx < baseTxns.size() && bound.includes(baseTxns[idx].timestamp)) {
    const auto &txn = baseTxns[idx];
    (void)ledger->transfer(txn.source, txn.target, txn.amount,
                           txn.session.channel);
    ++idx;
  }

  return idx;
}

} // namespace PhantomLedger::clearing
