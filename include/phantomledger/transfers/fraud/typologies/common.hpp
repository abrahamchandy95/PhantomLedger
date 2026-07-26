#pragma once

#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/synth/econ/nominal.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/fraud/engine.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::transfers::fraud::typologies {

[[nodiscard]] inline bool
appendBoundedTxn(const transactions::Factory &txf,
                 std::vector<transactions::Transaction> &out,
                 std::int32_t budget, const transactions::Draft &draft) {
  if (static_cast<std::int32_t>(out.size()) >= budget) {
    return false;
  }
  out.push_back(txf.make(draft));
  return true;
}

[[nodiscard]] inline bool
appendBoundedTxn(IllicitContext &ctx,
                 std::vector<transactions::Transaction> &out,
                 std::int32_t budget, const transactions::Draft &draft) {
  return appendBoundedTxn(ctx.execution.txf, out, budget, draft);
}

/// H1 step 2b (class F): realize a calibration-year fraud amount at
/// the event date's CPI level — fraud steals era dollars (authority
/// U-6). Chain math (haircuts, splits, floors) stays in calibration
/// dollars; the scale applies ONCE at each Draft's emission, so
/// behavioral floors bind identically in every era. Structuring is
/// class S and never calls this.
[[nodiscard]] inline double nominalAt(double amount, time::TimePoint ts) {
  return primitives::utils::roundMoney(
      amount * synth::econ::priceScale(time::toCalendarDate(ts).year));
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
