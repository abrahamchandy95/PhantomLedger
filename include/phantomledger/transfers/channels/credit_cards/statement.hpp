#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transactions/record.hpp"

#include <optional>
#include <span>

namespace PhantomLedger::transfers::credit_cards {

struct BillingTerms {
  int graceDays = 25;

  double minPaymentPct = 0.02;
  double minPaymentDollars = 25.0;

  double lateFee = 32.0;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::ge("graceDays", graceDays, 1); });
    r.check([&] { v::unit("minPaymentPct", minPaymentPct); });
    r.check([&] { v::nonNegative("minPaymentDollars", minPaymentDollars); });
    r.check([&] { v::nonNegative("lateFee", lateFee); });
  }
};

inline constexpr BillingTerms kDefaultBillingTerms{};

/// Fractional-day distance between two timestamps, clamped at zero.
[[nodiscard]] double intervalDays(time::TimePoint a,
                                  time::TimePoint b) noexcept;

/// Result of integrating a card's balance over one cycle.
struct BalanceSnapshot {
  double average;
  double ending;
};

[[nodiscard]] BalanceSnapshot
integrateBalance(const entity::Key &cardAccount, double openingBalance,
                 std::span<const transactions::Transaction> events,
                 time::TimePoint t0, time::TimePoint t1) noexcept;

[[nodiscard]] double minimumDue(const BillingTerms &terms,
                                double statementAbs) noexcept;

[[nodiscard]] std::optional<double> billableInterest(double rawAmount) noexcept;

} // namespace PhantomLedger::transfers::credit_cards
