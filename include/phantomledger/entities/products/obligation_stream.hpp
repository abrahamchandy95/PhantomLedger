#pragma once
/*
 * ObligationStream — append-only time-ordered store of obligation
 * events emitted by financial products.
 *
 * Writers (product builders) call append() during entity synthesis
 * in arbitrary order, then call sort() once. Readers call between()
 * to get a half-open span of events whose timestamps fall in
 * [start, endExcl).
 *
 * The returned span is only valid until the next mutation — the
 * obligations transfer generator reads the whole span in one pass
 * and never interleaves mutations with reads, so this is safe in
 * practice. The invariant is documented on between().
 */

#include "phantomledger/entities/products/event.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

#include <algorithm>
#include <span>
#include <utility>
#include <vector>

namespace PhantomLedger::entity::product {

class ObligationStream {
public:
  ObligationStream() = default;

  void append(ObligationEvent event) {
    sorted_ = false;
    events_.push_back(std::move(event));
  }

  /// Sort in place by timestamp. Idempotent: safe to call multiple
  /// times; no-op if already sorted and no new events arrived since.
  void sort() {
    if (sorted_) {
      return;
    }
    std::sort(events_.begin(), events_.end(),
              [](const ObligationEvent &a, const ObligationEvent &b) {
                return a.timestamp < b.timestamp;
              });
    sorted_ = true;
  }

  /// Half-open slice of events with timestamp in [start, endExcl).
  /// Requires sort() to have been called since the last append.
  /// Returns an empty span and leaves the stream unchanged if not.
  [[nodiscard]] std::span<const ObligationEvent>
  between(time::TimePoint start, time::TimePoint endExcl) const noexcept {
    if (!sorted_) {
      return {};
    }
    const auto first =
        std::lower_bound(events_.begin(), events_.end(), start,
                         [](const ObligationEvent &e, time::TimePoint t) {
                           return e.timestamp < t;
                         });
    const auto last =
        std::lower_bound(first, events_.end(), endExcl,
                         [](const ObligationEvent &e, time::TimePoint t) {
                           return e.timestamp < t;
                         });
    return {first, static_cast<std::size_t>(last - first)};
  }

  [[nodiscard]] std::size_t size() const noexcept { return events_.size(); }
  [[nodiscard]] bool empty() const noexcept { return events_.empty(); }
  [[nodiscard]] bool sorted() const noexcept { return sorted_; }

private:
  std::vector<ObligationEvent> events_;
  bool sorted_ = true; ///< trivially sorted when empty
};

} // namespace PhantomLedger::entity::product
