#pragma once
/*
 * Lease state machine.
 *
 * Tracks the lifecycle of a person's housing lease: landlord
 * assignment, lease tenure, base rent with compounding raises,
 * and lease transitions (move to new landlord with fresh rent).
 */

#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/recurring/growth.hpp"
#include "phantomledger/recurring/policy.hpp"

#include <algorithm>
#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace PhantomLedger::recurring {

/// Mutable lease state for one person.
struct Lease {
  entity::Key landlordAcct;
  time::TimePoint start;
  time::TimePoint end;
  double baseRent = 0.0;
  int moveIndex = 0;
};

/// Source callback for base rent draws.
using RentSource = std::function<double()>;

/// Narrow policy view used by lease logic.
struct LeaseRules {
  const TenurePolicy &tenure;
  const InflationPolicy &inflation;
  const RaisePolicy &raises;
};

struct LeaseInitInput {
  std::string_view payerKey;
  time::TimePoint startDate;
  std::span<const entity::Key> landlords;
  RentSource rentSource;
};

struct LeaseAdvanceInput {
  std::string_view payerKey;
  time::TimePoint now;
  std::span<const entity::Key> landlords;
  const Lease &previous;
  RentSource resetRentSource;
};

struct RentQuery {
  std::string_view payerKey;
  const Lease &state;
  time::TimePoint payDate;
};

namespace detail {

struct RentGrowthInput {
  std::string_view payerKey;
  time::TimePoint start;
  time::TimePoint now;
};

[[nodiscard]] inline double rentRealRaise(const random::RngFactory &factory,
                                          const RaisePolicy &raises,
                                          std::string_view key, int year) {
  auto rng = factory.rng({"rent_real_raise", key, std::to_string(year)});
  return growth::sampleNormalClamped(rng, raises.rent.mu, raises.rent.sigma,
                                     raises.rent.floor);
}

[[nodiscard]] inline double compoundRent(const LeaseRules &rules,
                                         const random::RngFactory &factory,
                                         const RentGrowthInput &input) {
  const int years = growth::anniversariesPassed(input.start, input.now);
  if (years <= 0) {
    return 1.0;
  }

  const auto startCal = time::toCalendarDate(input.start);
  double growthFactor = 1.0;

  for (int i = 0; i < years; ++i) {
    const double realRaise =
        rentRealRaise(factory, rules.raises, input.payerKey, startCal.year + i);
    growthFactor *= (1.0 + rules.inflation.annual + realRaise);
  }

  return growthFactor;
}

} // namespace detail

/// Bootstrap the initial lease state for a payer account.
[[nodiscard]] inline Lease initializeLease(const LeaseRules &rules,
                                           const random::RngFactory &factory,
                                           random::Rng &rng,
                                           const LeaseInitInput &input) {
  auto initRng = factory.rng({"lease_init", input.payerKey});

  const auto landlord = growth::pickOne(initRng, input.landlords);
  const auto [leaseStart, leaseEnd] = growth::sampleBackdatedInterval(
      initRng, input.startDate, rules.tenure.lease.min, rules.tenure.lease.max);

  const double baseRent =
      input.rentSource() * growth::sampleLognormalMultiplier(initRng, 0.05);

  // Advance the shared RNG to maintain determinism (mirrors Python).
  (void)rng.nextDouble();

  return Lease{
      .landlordAcct = landlord,
      .start = leaseStart,
      .end = leaseEnd,
      .baseRent = baseRent,
      .moveIndex = 0,
  };
}

/// Transition to a new lease with fresh landlord and base rent.
[[nodiscard]] inline Lease advanceLease(const LeaseRules &rules,
                                        const random::RngFactory &factory,
                                        random::Rng &rng,
                                        const LeaseAdvanceInput &input) {
  const auto landlord =
      growth::pickDifferent(rng, input.landlords, input.previous.landlordAcct);

  const auto [start, end] = growth::sampleForwardInterval(
      rng, input.now, rules.tenure.lease.min, rules.tenure.lease.max);

  auto resetRng = factory.rng({"lease_reset_rent", input.payerKey,
                               std::to_string(input.previous.moveIndex + 1)});

  const double baseRent = input.resetRentSource() *
                          growth::sampleLognormalMultiplier(resetRng, 0.05);

  return Lease{
      .landlordAcct = landlord,
      .start = start,
      .end = end,
      .baseRent = baseRent,
      .moveIndex = input.previous.moveIndex + 1,
  };
}

/// Calculate the rent for a specific month, including compounded growth.
[[nodiscard]] inline double calculateRent(const LeaseRules &rules,
                                          const random::RngFactory &factory,
                                          const RentQuery &query) {
  const double amount = query.state.baseRent *
                        detail::compoundRent(rules, factory,
                                             detail::RentGrowthInput{
                                                 .payerKey = query.payerKey,
                                                 .start = query.state.start,
                                                 .now = query.payDate,
                                             });

  return primitives::utils::roundMoney(std::max(1.0, amount));
}

} // namespace PhantomLedger::recurring
