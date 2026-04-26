#pragma once
/*
 * LocAccrualTracker — line-of-credit interest accrual.
 */

#include <cstdint>
#include <vector>

namespace PhantomLedger::clearing {

/// One matured LOC billing period, ready to be billed.
struct InterestAccrual {
  std::uint32_t accountIndex = 0;
  double interest = 0.0;
  std::int64_t timestamp = 0;
};

class LocAccrualTracker {
public:
  using Index = std::uint32_t;

  static constexpr std::int64_t kBillingPeriodSeconds = 30 * 86400;

  static constexpr double kYearSeconds = 365.25 * 86400.0;

  void initialize(Index count);
  [[nodiscard]] Index size() const noexcept { return size_; }

  // --- Per-account setup ---

  void enable(Index idx, double apr, int billingDay) noexcept;

  void disable(Index idx) noexcept;

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

  void copyStateFrom(const LocAccrualTracker &other) noexcept;

private:
  [[nodiscard]] bool isEnabled(Index idx) const noexcept;

  Index size_ = 0;

  std::vector<std::uint8_t> enabled_;
  std::vector<double> apr_;
  std::vector<std::int32_t> billingDay_;
  std::vector<double> dollarSecondsIntegral_;
  std::vector<std::int64_t> lastUpdateTs_;
  std::vector<std::int64_t> lastBillingTs_;
};

template <class CashFn>
void LocAccrualTracker::sweep(std::int64_t ts, CashFn &&currentCash,
                              std::vector<InterestAccrual> &out) {
  for (Index idx = 0; idx < size_; ++idx) {
    if (!isEnabled(idx)) {
      continue;
    }

    update(idx, currentCash(idx), ts);

    const auto lastBilling = lastBillingTs_[idx];
    if (lastBilling == 0) {
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
