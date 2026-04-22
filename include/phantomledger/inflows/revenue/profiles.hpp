#pragma once
/*
 * Each persona archetype (freelancer, smallbiz, HNW) has a
 * RevenuePersonaProfile that controls how many counterparties,
 * payments per month, median amounts, and quiet-month behavior
 * each revenue flow exhibits.
 */

namespace PhantomLedger::inflows::revenue {

namespace detail {

[[nodiscard]] constexpr bool unitProbability(double value) noexcept {
  return value >= 0.0 && value <= 1.0;
}

[[nodiscard]] constexpr bool nonNegative(double value) noexcept {
  return value >= 0.0;
}

[[nodiscard]] constexpr bool nonNegative(int value) noexcept {
  return value >= 0;
}

[[nodiscard]] constexpr bool orderedRange(int minValue, int maxValue) noexcept {
  return minValue <= maxValue;
}

} // namespace detail

/// Profile for a counterparty-based revenue flow (clients, platforms).
/// Controls how many counterparties to draw from the pool and how
/// many payments each produces per month.
struct CounterpartyRevenueProfile {
  double activeP = 0.0;
  int counterpartiesMin = 1;
  int counterpartiesMax = 1;
  int paymentsMin = 0;
  int paymentsMax = 0;
  double median = 0.0;
  double sigma = 0.0;

  [[nodiscard]] constexpr bool valid() const noexcept {
    return detail::unitProbability(activeP) && counterpartiesMin >= 1 &&
           detail::orderedRange(counterpartiesMin, counterpartiesMax) &&
           detail::nonNegative(paymentsMin) &&
           detail::orderedRange(paymentsMin, paymentsMax) &&
           detail::nonNegative(median) && detail::nonNegative(sigma);
  }
};

/// Profile for a single-source revenue flow (settlements, draws,
/// investments).
struct SingleSourceRevenueProfile {
  double activeP = 0.0;
  int paymentsMin = 0;
  int paymentsMax = 0;
  double median = 0.0;
  double sigma = 0.0;

  [[nodiscard]] constexpr bool valid() const noexcept {
    return detail::unitProbability(activeP) &&
           detail::nonNegative(paymentsMin) &&
           detail::orderedRange(paymentsMin, paymentsMax) &&
           detail::nonNegative(median) && detail::nonNegative(sigma);
  }
};

/// Controls whether a persona skips a given month entirely.
struct QuietMonthProfile {
  double probability = 0.0;
  double skipProbability = 0.60;

  [[nodiscard]] constexpr bool valid() const noexcept {
    return detail::unitProbability(probability) &&
           detail::unitProbability(skipProbability);
  }
};

/// Complete revenue profile for one persona archetype.
struct RevenuePersonaProfile {
  CounterpartyRevenueProfile client{};
  CounterpartyRevenueProfile platform{};
  SingleSourceRevenueProfile settlement{};
  SingleSourceRevenueProfile ownerDraw{};
  SingleSourceRevenueProfile investment{};
  QuietMonthProfile quietMonth{};

  [[nodiscard]] constexpr bool valid() const noexcept {
    return client.valid() && platform.valid() && settlement.valid() &&
           ownerDraw.valid() && investment.valid() && quietMonth.valid();
  }
};

} // namespace PhantomLedger::inflows::revenue
