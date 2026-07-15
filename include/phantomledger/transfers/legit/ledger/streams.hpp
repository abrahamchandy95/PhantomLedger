#pragma once

#include "phantomledger/taxonomies/channels/predicates.hpp"
#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

namespace PhantomLedger::transfers::legit::ledger {

namespace detail {

/// Funds-transfer replay key — (timestamp, source, target, amount).
[[nodiscard]] inline bool
fundsLess(const transactions::Transaction &a,
          const transactions::Transaction &b) noexcept {
  if (a.timestamp != b.timestamp) {
    return a.timestamp < b.timestamp;
  }
  if (a.source != b.source) {
    return a.source < b.source;
  }
  if (a.target != b.target) {
    return a.target < b.target;
  }
  return a.amount < b.amount;
}

[[nodiscard]] inline bool
timestampLess(const transactions::Transaction &a,
              const transactions::Transaction &b) noexcept {
  return a.timestamp < b.timestamp;
}

} // namespace detail

[[nodiscard]] inline std::vector<transactions::Transaction>
sortForReplay(std::span<const transactions::Transaction> txns) {
  std::vector<transactions::Transaction> out(txns.begin(), txns.end());
  std::ranges::sort(out, detail::fundsLess);
  return out;
}

[[nodiscard]] inline std::vector<transactions::Transaction>
sortForReplay(std::vector<transactions::Transaction> &&txns) {
  std::ranges::sort(txns, detail::fundsLess);
  return std::move(txns);
}

/// Merge a new (unsorted) batch into an already-replay-sorted prefix.
[[nodiscard]] inline std::vector<transactions::Transaction>
mergeReplaySorted(std::vector<transactions::Transaction> existing,
                  std::span<const transactions::Transaction> newItems) {
  if (newItems.empty()) {
    return existing;
  }

  std::vector<transactions::Transaction> sortedNew(newItems.begin(),
                                                   newItems.end());
  std::ranges::sort(sortedNew, detail::fundsLess);

  if (existing.empty()) {
    return sortedNew;
  }

  std::vector<transactions::Transaction> merged;
  merged.reserve(existing.size() + sortedNew.size());
  std::ranges::merge(existing, sortedNew, std::back_inserter(merged),
                     detail::fundsLess);
  return merged;
}

class TxnStreams {
public:
  TxnStreams() = default;

  void reserve(std::size_t total) {
    screened_.reserve(total);
    replayReady_.reserve(total);
  }

  [[nodiscard]] const std::vector<transactions::Transaction> &
  screened() const noexcept {
    return screened_;
  }

  [[nodiscard]] const std::vector<transactions::Transaction> &
  paydayInbound() const noexcept {
    return paydayInbound_;
  }

  [[nodiscard]] const std::vector<transactions::Transaction> &
  replayReady() const noexcept {
    return replayReady_;
  }

  [[nodiscard]] std::vector<transactions::Transaction> &&
  takeReplayReady() noexcept {
    return std::move(replayReady_);
  }

  void add(std::vector<transactions::Transaction> items) {
    if (items.empty()) {
      return;
    }

    for (const auto &txn : items) {
      if (channels::isPaydayInbound(txn.session.channel)) {
        paydayInbound_.push_back(txn);
      }
    }

    addSortedView(screened_, std::span<const transactions::Transaction>(items),
                  detail::timestampLess);
    addSortedView(replayReady_, std::move(items), detail::fundsLess);
  }

private:
  template <class Less>
  static void addSortedView(std::vector<transactions::Transaction> &dst,
                            std::vector<transactions::Transaction> items,
                            Less less) {
    if (items.empty()) {
      return;
    }
    std::ranges::sort(items, less);

    if (dst.empty()) {
      dst = std::move(items);
      return;
    }

    const auto seam = dst.size();
    dst.reserve(seam + items.size());
    dst.insert(dst.end(), std::make_move_iterator(items.begin()),
               std::make_move_iterator(items.end()));
    std::inplace_merge(dst.begin(), dst.begin() + seam, dst.end(), less);
  }

  template <class Less>
  static void addSortedView(std::vector<transactions::Transaction> &dst,
                            std::span<const transactions::Transaction> items,
                            Less less) {
    if (items.empty()) {
      return;
    }
    std::vector<transactions::Transaction> sorted(items.begin(), items.end());
    std::ranges::sort(sorted, less);

    if (dst.empty()) {
      dst = std::move(sorted);
      return;
    }

    const auto seam = dst.size();
    dst.reserve(seam + sorted.size());
    dst.insert(dst.end(), std::make_move_iterator(sorted.begin()),
               std::make_move_iterator(sorted.end()));
    std::inplace_merge(dst.begin(), dst.begin() + seam, dst.end(), less);
  }

  std::vector<transactions::Transaction> screened_;
  std::vector<transactions::Transaction> replayReady_;
  std::vector<transactions::Transaction> paydayInbound_;
};

} // namespace PhantomLedger::transfers::legit::ledger
