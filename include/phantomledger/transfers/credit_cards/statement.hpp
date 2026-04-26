#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/credit_cards/policy/issuer.hpp"

#include <optional>
#include <span>

namespace PhantomLedger::transfers::credit_cards {

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

[[nodiscard]] double minimumDue(const BillingPolicy &policy,
                                double statementAbs) noexcept;

[[nodiscard]] std::optional<double> billableInterest(double rawAmount) noexcept;

} // namespace PhantomLedger::transfers::credit_cards
