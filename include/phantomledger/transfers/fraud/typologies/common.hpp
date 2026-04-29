#pragma once

#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/fraud/engine.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::transfers::fraud::typologies {

[[nodiscard]] inline bool
appendBoundedTxn(IllicitContext &ctx,
                 std::vector<transactions::Transaction> &out,
                 std::int32_t budget, const transactions::Draft &draft) {
  if (static_cast<std::int32_t>(out.size()) >= budget) {
    return false;
  }
  out.push_back(ctx.execution.txf.make(draft));
  return true;
}

template <class T>
[[nodiscard]] inline const T &pickOne(random::Rng &rng,
                                      std::span<const T> items) {
  return items[rng.choiceIndex(items.size())];
}

template <class T>
[[nodiscard]] inline const T &pickOne(random::Rng &rng,
                                      const std::vector<T> &items) {
  return items[rng.choiceIndex(items.size())];
}

/// Pick `k` distinct elements from `items` (or up to `items.size()` if k is
/// larger). Mirrors Python's rng.choice_k(items, k, replace=False).
template <class T>
[[nodiscard]] inline std::vector<T>
pickK(random::Rng &rng, const std::vector<T> &items, std::size_t k) {
  std::vector<T> out;
  if (items.empty() || k == 0) {
    return out;
  }
  const auto effective = std::min(k, items.size());
  const auto indices =
      rng.choiceIndices(items.size(), effective, /*replace=*/false);
  out.reserve(indices.size());
  for (const auto idx : indices) {
    out.push_back(items[idx]);
  }
  return out;
}

} // namespace PhantomLedger::transfers::fraud::typologies
