#include "phantomledger/transactions/clearing/loc_accrual.hpp"

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
}

bool LocAccrualTracker::isEnabled(Index idx) const noexcept {
  return idx < size_ && enabled_[idx] != 0;
}

void LocAccrualTracker::enable(Index idx, double apr, int billingDay) noexcept {
  assert(idx < size_);
  enabled_[idx] = 1;
  apr_[idx] = apr;
  billingDay_[idx] = static_cast<std::int32_t>(billingDay);
  // Fresh state: if the account was previously a different protection
  // type, any stale integral/last-update values are meaningless here.
  dollarSecondsIntegral_[idx] = 0.0;
  lastUpdateTs_[idx] = 0;
  lastBillingTs_[idx] = 0;
}

void LocAccrualTracker::disable(Index idx) noexcept {
  assert(idx < size_);
  enabled_[idx] = 0;
  apr_[idx] = 0.0;
  billingDay_[idx] = 0;
  dollarSecondsIntegral_[idx] = 0.0;
  lastUpdateTs_[idx] = 0;
  lastBillingTs_[idx] = 0;
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

  const auto lastTs = lastUpdateTs_[idx];
  // Only accumulate during periods where the balance was negative;
  // positive balances accrue no LOC interest.
  if (lastTs != 0 && ts > lastTs && preCash < 0.0) {
    const double elapsed = static_cast<double>(ts - lastTs);
    dollarSecondsIntegral_[idx] += (-preCash) * elapsed;
  }
  lastUpdateTs_[idx] = ts;
}

void LocAccrualTracker::copyStateFrom(const LocAccrualTracker &other) noexcept {
  assert(size_ == other.size_);
  dollarSecondsIntegral_ = other.dollarSecondsIntegral_;
  lastUpdateTs_ = other.lastUpdateTs_;
  lastBillingTs_ = other.lastBillingTs_;
}

} // namespace PhantomLedger::clearing
