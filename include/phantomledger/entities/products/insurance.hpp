#pragma once

/*
 * Insurance policy product adapter.
 *
 * The ownership decision and premium amount sampling happen in the
 * product builder. This header only defines the shape of the data
 * that transfers::insurance::generate() consumes.
 *
 * Unlike loan products, insurance does not implement scheduled_events();
 * the insurance transfer generator schedules its own premium and claim
 * events directly from a WindowCalendar. These types exist so:
 *   - The portfolio is complete (every product a person holds is visible)
 *   - Downstream consumers (ML features, analytics) can check policy
 *     presence
 *   - Future life events can reference and modify coverage
 *
 * Statistics the defaults are tuned against:
 *   - Auto: 92% of US households own a vehicle (Census 2024)
 *   - Home: avg $163/month premium (Ramsey 2025)
 *   - Auto: avg $225/month premium (Bankrate 2026)
 *   - Life: 52% of Americans have coverage (NW Mutual 2024)
 *   - Auto claims: ~4.2 per 100 drivers/year (III 2024)
 *   - Home claims: ~5-6% of policies/year (Triple-I 2024)
 */

#include "phantomledger/entities/identifier/key.hpp"

#include <cassert>
#include <cstdint>
#include <optional>

namespace PhantomLedger::entity::product {

enum class PolicyType : std::uint8_t {
  auto_ = 0,
  home = 1,
  life = 2,
};

/// A single active insurance policy for one coverage type.
///
/// `billingDay` is the day-of-month the monthly premium is debited;
/// it is clamped to [1, 28] so February wrap behavior is irrelevant.
/// `annualClaimP` is the probability of a claim per policy-year; the
/// transfer generator converts this into a window probability using
/// a Bernoulli-per-year approximation.
struct InsurancePolicy {
  PolicyType policyType = PolicyType::auto_;
  entity::Key carrierAcct{};
  double monthlyPremium = 0.0;
  std::int32_t billingDay = 1;
  double annualClaimP = 0.0;

  [[nodiscard]] constexpr bool ok() const noexcept {
    return monthlyPremium > 0.0 && billingDay >= 1 && billingDay <= 28 &&
           annualClaimP >= 0.0 && annualClaimP <= 1.0;
  }
};

/// Complete insurance coverage for one person.
///
/// Any subset of the three slots may be empty. Ordering of the slots
/// is fixed so the transfer generator can iterate deterministically
/// without constructing an intermediate list.
struct InsuranceHoldings {
  std::optional<InsurancePolicy> auto_;
  std::optional<InsurancePolicy> home;
  std::optional<InsurancePolicy> life;

  [[nodiscard]] constexpr int activeCount() const noexcept {
    return (auto_.has_value() ? 1 : 0) + (home.has_value() ? 1 : 0) +
           (life.has_value() ? 1 : 0);
  }

  [[nodiscard]] constexpr double totalMonthlyPremium() const noexcept {
    double t = 0.0;
    if (auto_.has_value()) {
      t += auto_->monthlyPremium;
    }
    if (home.has_value()) {
      t += home->monthlyPremium;
    }
    if (life.has_value()) {
      t += life->monthlyPremium;
    }
    return t;
  }

  [[nodiscard]] constexpr bool hasAny() const noexcept {
    return activeCount() > 0;
  }
};

// --- Constructor helpers ---

[[nodiscard]] constexpr InsurancePolicy autoPolicy(entity::Key carrierAcct,
                                                   double monthlyPremium,
                                                   std::int32_t billingDay,
                                                   double claimP) noexcept {
  return {PolicyType::auto_, carrierAcct, monthlyPremium, billingDay, claimP};
}

[[nodiscard]] constexpr InsurancePolicy homePolicy(entity::Key carrierAcct,
                                                   double monthlyPremium,
                                                   std::int32_t billingDay,
                                                   double claimP) noexcept {
  return {PolicyType::home, carrierAcct, monthlyPremium, billingDay, claimP};
}

[[nodiscard]] constexpr InsurancePolicy
lifePolicy(entity::Key carrierAcct, double monthlyPremium,
           std::int32_t billingDay) noexcept {
  // Death events are not modeled; claim probability is fixed at 0.
  return {PolicyType::life, carrierAcct, monthlyPremium, billingDay, 0.0};
}

} // namespace PhantomLedger::entity::product
