#include "phantomledger/transfers/credit_cards/statement.hpp"

#include "phantomledger/primitives/utils/rounding.hpp"

#include <algorithm>
#include <cmath>

namespace PhantomLedger::transfers::credit_cards {
namespace {

inline constexpr double kSecondsPerDay = 86400.0;

[[nodiscard]] double
balanceDelta(const entity::Key &cardAccount,
             const transactions::Transaction &txn) noexcept {
  if (txn.source == cardAccount) {
    return -txn.amount;
  }
  if (txn.target == cardAccount) {
    return +txn.amount;
  }
  return 0.0;
}

} // namespace

double intervalDays(time::TimePoint a, time::TimePoint b) noexcept {
  if (b <= a) {
    return 0.0;
  }
  const auto secs = time::toEpochSeconds(b) - time::toEpochSeconds(a);
  return static_cast<double>(secs) / kSecondsPerDay;
}

BalanceSnapshot
integrateBalance(const entity::Key &cardAccount, double openingBalance,
                 std::span<const transactions::Transaction> events,
                 time::TimePoint t0, time::TimePoint t1) noexcept {
  if (t1 <= t0) {
    return {openingBalance, openingBalance};
  }

  double balance = openingBalance;
  double dollarDayIntegral = 0.0;
  time::TimePoint prev = t0;

  for (const auto &ev : events) {
    const auto ts = time::fromEpochSeconds(ev.timestamp);
    if (ts < t0 || ts >= t1) {
      continue;
    }
    dollarDayIntegral += balance * intervalDays(prev, ts);
    balance += balanceDelta(cardAccount, ev);
    prev = ts;
  }

  // Tail segment from the last event up to the cycle close.
  dollarDayIntegral += balance * intervalDays(prev, t1);

  const double total = intervalDays(t0, t1);
  if (total <= 0.0) {
    return {balance, balance};
  }
  return {dollarDayIntegral / total, balance};
}

double minimumDue(const BillingPolicy &policy, double statementAbs) noexcept {
  return std::max(policy.minPaymentDollars,
                  policy.minPaymentPct * statementAbs);
}

std::optional<double> billableInterest(double rawAmount) noexcept {
  if (!std::isfinite(rawAmount) || rawAmount <= 0.01) {
    return std::nullopt;
  }
  return primitives::utils::roundMoney(rawAmount);
}

} // namespace PhantomLedger::transfers::credit_cards
