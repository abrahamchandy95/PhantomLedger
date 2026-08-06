#include "phantomledger/transactions/clearing/loc_accrual.hpp"

#include <algorithm>
#include <cassert>

namespace PhantomLedger::clearing {

void LocAccrualTracker::initialize(Index count) {
  size_ = count;
  enabled_.assign(count, 0);
  apr_.assign(count, 0.0);
  billingDay_.assign(count, 0);
  dollarSecondsIntegral_.assign(count, 0.0);
  lastUpdateTs_.assign(count, 0);
  lastBillingTs_.assign(count, 0);
  queuedDueTs_.assign(count, kNotQueued);
  billingQueue_ = {};
  dueScratch_.clear();
}

void LocAccrualTracker::scheduleBilling(Index idx, std::int64_t dueTs) {
  queuedDueTs_[idx] = dueTs;
  billingQueue_.emplace(dueTs, idx);
}

bool LocAccrualTracker::isEnabled(Index idx) const noexcept {
  return idx < size_ && enabled_[idx] != 0;
}

void LocAccrualTracker::enable(Index idx, double apr, int billingDay) {
  assert(idx < size_);
  enabled_[idx] = 1;
  apr_[idx] = apr;
  billingDay_[idx] = static_cast<std::int32_t>(billingDay);
  /* Fresh state: if the account carried a different protection type, any
   * stale integral/last-update values are meaningless here. */
  dollarSecondsIntegral_[idx] = 0.0;
  lastUpdateTs_[idx] = 0;
  lastBillingTs_[idx] = 0;
  /* Due at 0 so the FIRST sweep after this pops the slot and takes the
   * `lastBillingTs_ == 0` arm. That is where the billing clock starts;
   * starting it anywhere else shifts every subsequent maturity for this
   * account. Supersedes any entry a prior enable() left queued. */
  scheduleBilling(idx, 0);
}

void LocAccrualTracker::disable(Index idx) {
  assert(idx < size_);
  enabled_[idx] = 0;
  apr_[idx] = 0.0;
  billingDay_[idx] = 0;
  dollarSecondsIntegral_[idx] = 0.0;
  lastUpdateTs_[idx] = 0;
  lastBillingTs_[idx] = 0;
  /* Lazy deletion: the queue entry stays, and sweep() drops it on sight
   * because the token no longer matches. */
  queuedDueTs_[idx] = kNotQueued;
}

bool LocAccrualTracker::enabled(Index idx) const noexcept {
  return isEnabled(idx);
}

double LocAccrualTracker::apr(Index idx) const noexcept {
  assert(idx < size_);
  return apr_[idx];
}

int LocAccrualTracker::billingDay(Index idx) const noexcept {
  assert(idx < size_);
  return billingDay_[idx];
}

void LocAccrualTracker::update(Index idx, double preCash,
                               std::int64_t ts) noexcept {
  if (!isEnabled(idx)) {
    return;
  }

  /* `ts == 0` IS A SENTINEL, NOT AN INSTANT: it is the untimestamped
   * `Ledger::transfer` overload. Stamping it would REWIND `lastUpdateTs_` to
   * the epoch and accrue the following interval from 1970. Ignore it. */
  if (ts == 0) {
    return;
  }

  const auto lastTs = lastUpdateTs_[idx];
  /* Only accumulate over periods where the balance was negative; positive
   * balances accrue no LOC interest. */
  if (lastTs != 0 && ts > lastTs && preCash < 0.0) {
    const double elapsed = static_cast<double>(ts - lastTs);
    dollarSecondsIntegral_[idx] += (-preCash) * elapsed;
  }
  lastUpdateTs_[idx] = ts;
}

void LocAccrualTracker::copyStateFrom(const LocAccrualTracker &other) {
  /* Copies accrual STATE only; enablement must already match between the two
   * trackers — the clone/restore pattern restores onto a book with identical
   * account setup.
   *
   * THE BILLING QUEUE AND ITS TOKENS MUST TRAVEL HERE. They are derived from
   * `lastBillingTs_`, so restoring the timestamps while keeping this book's
   * own queue leaves the two disagreeing about when each slot next matures —
   * a monolithic/windowed engine divergence no golden would localise to this
   * file. */
  assert(size_ == other.size_);
  dollarSecondsIntegral_ = other.dollarSecondsIntegral_;
  lastUpdateTs_ = other.lastUpdateTs_;
  lastBillingTs_ = other.lastBillingTs_;
  queuedDueTs_ = other.queuedDueTs_;
  billingQueue_ = other.billingQueue_;
}

} // namespace PhantomLedger::clearing
