#pragma once

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
