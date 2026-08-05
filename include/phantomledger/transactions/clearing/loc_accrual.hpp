#pragma once

#include "phantomledger/primitives/time/constants.hpp"

#include <algorithm>
#include <cstdint>
#include <queue>
#include <utility>
#include <vector>

namespace PhantomLedger::clearing {

/// One matured Line Of Credit billing period, ready to be billed.
struct InterestAccrual {
  std::uint32_t accountIndex = 0;
  double interest = 0.0;
  std::int64_t timestamp = 0;
};

class LocAccrualTracker {
public:
  using Index = std::uint32_t;

  static constexpr std::int64_t kBillingPeriodSeconds =
      30 * ::PhantomLedger::time::kSecondsPerDay;

  static constexpr double kYearSeconds =
      365.25 * ::PhantomLedger::time::kSecondsPerDay;

  void initialize(Index count);
  [[nodiscard]] Index size() const noexcept { return size_; }

  // --- Per-account setup ---

  // Not noexcept: both publish into the billing queue, which may reallocate.
  void enable(Index idx, double apr, int billingDay);

  void disable(Index idx);

  [[nodiscard]] bool enabled(Index idx) const noexcept;
  [[nodiscard]] double apr(Index idx) const noexcept;
  [[nodiscard]] int billingDay(Index idx) const noexcept;

  // --- Integration ---

  void update(Index idx, double preCash, std::int64_t ts) noexcept;

  // --- Billing sweep ---

  template <class CashFn>
  void sweep(std::int64_t ts, CashFn &&currentCash,
             std::vector<InterestAccrual> &out);

  // --- Snapshot for clone/restore ---

  void copyStateFrom(const LocAccrualTracker &other);

private:
  [[nodiscard]] bool isEnabled(Index idx) const noexcept;

  // Publish `idx` into the billing queue as due at `dueTs`, superseding any
  // entry already queued for it. `queuedDueTs_` is the authority: a popped
  // entry whose key disagrees is a stale duplicate left by disable() or by a
  // re-enable, and is discarded. That is lazy deletion — std::priority_queue
  // cannot erase from the middle, and enable()/disable() run at setup rates
  // while pops run per replayed row.
  void scheduleBilling(Index idx, std::int64_t dueTs);

  Index size_ = 0;

  std::vector<std::uint8_t> enabled_;
  std::vector<double> apr_;
  std::vector<std::int32_t> billingDay_;
  std::vector<double> dollarSecondsIntegral_;
  std::vector<std::int64_t> lastUpdateTs_;
  std::vector<std::int64_t> lastBillingTs_;

  // loc-accrual-perf-2026-08: the due time currently published in
  // `billingQueue_` for each slot, or kNotQueued. Never read as a schedule —
  // only as the validity token for a popped entry.
  static constexpr std::int64_t kNotQueued = -1;
  std::vector<std::int64_t> queuedDueTs_;

  using BillingEntry = std::pair<std::int64_t, Index>;
  std::priority_queue<BillingEntry, std::vector<BillingEntry>,
                      std::greater<BillingEntry>>
      billingQueue_;

  // Reused across sweeps so the per-row path never allocates.
  std::vector<Index> dueScratch_;
};

// loc-accrual-perf-2026-08: THIS RAN ONCE PER REPLAYED ROW AND SCANNED EVERY
// ENABLED SLOT, so its cost was rows x accounts — and both factors are linear
// in population, making the whole generator O(population^2). Measured at
// population 4,000 over 180 days: 1.38 BILLION slot visits to produce 8,076
// billing events, 98.9% of total process samples. Population scaling was
// 2.19x / 3.09x / 3.50x / 3.76x per doubling, converging on 4x.
//
// The integrand is piecewise constant with jumps ONLY at an account's own
// postings, so a Riemann sum over those jump points is EXACT and every
// additional subdivision point the row clock contributed was redundant — it
// changed the float rounding of the sum without changing the integral. The
// integral is therefore rolled forward lazily (at each posting via
// `update`, and here at billing) rather than sliced per row, and billing is
// driven by a due-time min-heap instead of a scan.
//
// EXACTNESS OF THE BILLING INSTANT IS PRESERVED. The scan billed when
// `ts - lastBilling >= kBillingPeriodSeconds`; the heap pops when
// `dueTs <= ts` with `dueTs = lastBilling + kBillingPeriodSeconds`, which is
// the same predicate. Accounts maturing in one sweep are collected and
// processed in ASCENDING SLOT ORDER, reproducing the scan's `out` sequence —
// `out` drives `debitAndEmit`, so its order is the emitted row order.
//
// The dollar-seconds sum is fewer, larger terms than before, so interest
// amounts move in their low bits. That is a golden-moving change and the
// direction is toward LESS accumulated rounding, not more.
template <class CashFn>
void LocAccrualTracker::sweep(std::int64_t ts, CashFn &&currentCash,
                              std::vector<InterestAccrual> &out) {
  dueScratch_.clear();

  while (!billingQueue_.empty() && billingQueue_.top().first <= ts) {
    const auto [dueTs, idx] = billingQueue_.top();
    billingQueue_.pop();
    if (!isEnabled(idx) || queuedDueTs_[idx] != dueTs) {
      continue; // stale duplicate — see scheduleBilling
    }
    queuedDueTs_[idx] = kNotQueued;
    dueScratch_.push_back(idx);
  }

  if (dueScratch_.empty()) {
    return;
  }

  std::sort(dueScratch_.begin(), dueScratch_.end());

  for (const Index idx : dueScratch_) {
    update(idx, currentCash(idx), ts);

    // First maturity after enable(): the scan's `lastBilling == 0` arm, which
    // starts the clock at the first sweep the slot is seen in and bills
    // nothing. enable() queues at 0 precisely so that lands here.
    if (lastBillingTs_[idx] == 0) {
      lastBillingTs_[idx] = ts;
      scheduleBilling(idx, ts + kBillingPeriodSeconds);
      continue;
    }

    const double interest =
        dollarSecondsIntegral_[idx] * apr_[idx] / kYearSeconds;

    if (interest > 0.0) {
      out.push_back(InterestAccrual{
          .accountIndex = idx,
          .interest = interest,
          .timestamp = ts,
      });
    }

    dollarSecondsIntegral_[idx] = 0.0;
    lastBillingTs_[idx] = ts;
    scheduleBilling(idx, ts + kBillingPeriodSeconds);
  }
}

} // namespace PhantomLedger::clearing
