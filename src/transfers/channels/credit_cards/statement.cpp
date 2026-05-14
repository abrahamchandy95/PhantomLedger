#include "phantomledger/transfers/channels/credit_cards/statement.hpp"

#include "phantomledger/primitives/time/constants.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"

#include <algorithm>
#include <cmath>

namespace PhantomLedger::transfers::credit_cards {
namespace {

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
  return static_cast<double>(secs) / time::kSecondsPerDay;
}

BalanceSnapshot
integrateBalance(const entity::Key &cardAccount, double openingBalance,
                 std::span<const transactions::Transaction> events,
                 time::HalfOpenInterval cycle) noexcept {
  if (cycle.endExcl <= cycle.start) {
    return {openingBalance, openingBalance};
  }

  double balance = openingBalance;
  double dollarDayIntegral = 0.0;
  time::TimePoint prev = cycle.start;

  for (const auto &ev : events) {
    const auto ts = time::fromEpochSeconds(ev.timestamp);
    if (ts < cycle.start || ts >= cycle.endExcl) {
      continue;
    }
    dollarDayIntegral += balance * intervalDays(prev, ts);
    balance += balanceDelta(cardAccount, ev);
    prev = ts;
  }

  // Tail segment from the last event up to the cycle close.
  dollarDayIntegral += balance * intervalDays(prev, cycle.endExcl);

  const double total = intervalDays(cycle.start, cycle.endExcl);
  if (total <= 0.0) {
    return {balance, balance};
  }
  return {dollarDayIntegral / total, balance};
}

double minimumDue(const BillingTerms &terms, double statementAbs) noexcept {
  return std::max(terms.minPaymentDollars, terms.minPaymentPct * statementAbs);
}

std::optional<double> billableInterest(double rawAmount) noexcept {
  if (!std::isfinite(rawAmount) || rawAmount <= 0.01) {
    return std::nullopt;
  }
  return primitives::utils::roundMoney(rawAmount);
}

} // namespace PhantomLedger::transfers::credit_cards
