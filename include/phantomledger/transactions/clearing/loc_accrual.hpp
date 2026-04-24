#pragma once
/*
 * LocAccrualTracker — line-of-credit interest accrual.
 *
 * Owns the per-account LOC state that the core Ledger doesn't need
 * in order to move money:
 *
 *   - apr                        : annualized rate
 *   - billingDay                 : day of month (1-28), reserved for
 *                                  future calendar-aware billing
 *   - dollarSecondsIntegral      : running sum of -cash * dt while
 *                                  cash has been negative
 *   - lastUpdateTs               : epoch seconds of last integration
 *   - lastBillingTs              : epoch seconds of last interest
 *                                  emission (0 before first sweep)
 *
 * The tracker has two responsibilities:
 *
 *   1. `update(idx, cash, ts)` rolls the dollar-seconds integral
 *      forward from lastUpdateTs_ to ts using the old (pre-transfer)
 *      cash value.
 *
 *   2. `sweep(ts)` returns every account whose billing period has
 *      elapsed, with the computed interest and the integral it was
 *      drawn from. It resets the integral and advances lastBillingTs_
 *      as a side effect; the caller is responsible for debiting the
 *      ledger and emitting a LiquidityEvent.
 *
 * Keeping the tracker ignorant of Ledger / LiquiditySink lets it be
 * unit-tested as a pure state machine.
 */

#include <cstdint>
#include <vector>

namespace PhantomLedger::clearing {

/// One matured LOC billing period, ready to be billed.
struct InterestAccrual {
  std::uint32_t accountIndex = 0;
  double interest = 0.0; ///< computed dollars, already APR-weighted
  std::int64_t timestamp = 0;
};

class LocAccrualTracker {
public:
  using Index = std::uint32_t;

  /// Billing period used when `sweep(ts)` decides if an account is
  /// due. 30 days matches a typical monthly statement cycle. Exposed
  /// as a constant so tests can reason about it without reaching
  /// into the .cpp.
  static constexpr std::int64_t kBillingPeriodSeconds = 30 * 86400;

  /// Seconds in a 365.25-day year, used to convert APR * dollar-seconds
  /// into a dollar interest amount.
  static constexpr double kYearSeconds = 365.25 * 86400.0;

  void initialize(Index count);
  [[nodiscard]] Index size() const noexcept { return size_; }

  // --- Per-account setup ---

  /// Mark `idx` as a LOC-protected account. Required before `update`
  /// or `sweep` will do any work on it.
  void enable(Index idx, double apr, int billingDay) noexcept;

  /// Turn off LOC tracking for `idx` and forget any accrued state.
  /// Called when `setProtection` switches an account away from LOC.
  void disable(Index idx) noexcept;

  [[nodiscard]] bool enabled(Index idx) const noexcept;
  [[nodiscard]] double apr(Index idx) const noexcept;
  [[nodiscard]] int billingDay(Index idx) const noexcept;

  // --- Integration ---

  /// Roll the dollar-seconds integral for `idx` forward to `ts`,
  /// using `preCash` as the balance that was held during the window
  /// [lastUpdateTs_, ts). Non-LOC accounts are no-ops.
  ///
  /// Call before any transfer that might change the source/dest
  /// balance, so the integral reflects the pre-transfer balance over
  /// the elapsed time.
  void update(Index idx, double preCash, std::int64_t ts) noexcept;

  // --- Billing sweep ---

  /// For every LOC account whose billing period has elapsed since
  /// `lastBillingTs_`, compute interest from the current integral,
  /// reset the integral, advance `lastBillingTs_`, and emit a matured
  /// InterestAccrual into `out`. Accounts whose lastBillingTs_ is 0
  /// are anchored silently (no emission; first sweep just starts the
  /// clock).
  ///
  /// `out` is appended to, not cleared. The caller is expected to
  /// feed `preCash(idx)` in for each account via `currentCash` so the
  /// tracker can integrate up to `ts` before billing.
  template <class CashFn>
  void sweep(std::int64_t ts, CashFn &&currentCash,
             std::vector<InterestAccrual> &out);

  // --- Snapshot for clone/restore ---
  //
  // The ledger's clone/restore path needs to copy these so that a
  // re-replay starts from the same integrals. Exposed as explicit
  // member-copies instead of a friendship backdoor.

  void copyStateFrom(const LocAccrualTracker &other) noexcept;

private:
  [[nodiscard]] bool isEnabled(Index idx) const noexcept;

  Index size_ = 0;

  std::vector<std::uint8_t> enabled_; ///< 0/1 flags
  std::vector<double> apr_;
  std::vector<std::int32_t> billingDay_;
  std::vector<double> dollarSecondsIntegral_;
  std::vector<std::int64_t> lastUpdateTs_;
  std::vector<std::int64_t> lastBillingTs_;
};

// -----------------------------------------------------------------------------
// Template definition kept in the header so the Ledger .cpp can
// instantiate it with a lambda closing over its own cash_ vector
// without a virtual call.
// -----------------------------------------------------------------------------

template <class CashFn>
void LocAccrualTracker::sweep(std::int64_t ts, CashFn &&currentCash,
                              std::vector<InterestAccrual> &out) {
  for (Index idx = 0; idx < size_; ++idx) {
    if (!isEnabled(idx)) {
      continue;
    }

    // Roll the integral forward to `ts` using the account's current
    // cash value (which is the balance held through the period
    // ending now).
    update(idx, currentCash(idx), ts);

    const auto lastBilling = lastBillingTs_[idx];
    if (lastBilling == 0) {
      // Anchor the billing clock on first sweep; don't emit.
      lastBillingTs_[idx] = ts;
      continue;
    }

    if (ts - lastBilling < kBillingPeriodSeconds) {
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
  }
}

} // namespace PhantomLedger::clearing
