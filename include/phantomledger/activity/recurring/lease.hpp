#pragma once

#include "phantomledger/activity/recurring/growth.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

#include <algorithm>
#include <functional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace PhantomLedger::activity::recurring {

/// Mutable lease state for one person.
struct Lease {
  entity::Key landlordAcct;
  time::TimePoint start;
  time::TimePoint end;
  double baseRent = 0.0;
  int moveIndex = 0;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;

    r.check([&] { v::finite("baseRent", baseRent); });
    r.check([&] { v::positive("baseRent", baseRent); });
    r.check([&] { v::nonNegative("moveIndex", moveIndex); });
  }
};

/// Source callback for base rent draws.
using RentSource = std::function<double()>;

struct RentGrowthRules {
  growth::CompoundRules annual{
      .annualInflation = 0.025,
      .annualRaise =
          growth::GrowthDist{
              .mu = 0.020,
              .sigma = 0.015,
              .floor = -0.01,
          },
  };

  void validate(primitives::validate::Report &r) const { annual.validate(r); }
};

/// Rules owned by lease generation and rent growth.
struct LeaseRules {
  growth::Range tenure{
      .min = 1.0,
      .max = 3.0,
  };

  RentGrowthRules growth{};

  void validate(primitives::validate::Report &r) const {
    tenure.validate(r);
    growth.validate(r);
  }
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

namespace internal {

inline void requireKey(std::string_view key, std::string_view field) {
  if (key.empty()) {
    throw std::invalid_argument(std::string(field) + " is required");
  }
}

inline void requireRentSource(const RentSource &source,
                              std::string_view field) {
  if (!source) {
    throw std::invalid_argument(std::string(field) + " is required");
  }
}

inline void requireRentAmount(double amount, std::string_view field) {
  namespace v = primitives::validate;

  v::finite(field, amount);
  v::positive(field, amount);
}

[[nodiscard]] inline double sampleBaseRent(random::Rng &rng,
                                           const RentSource &source,
                                           std::string_view sourceField) {
  requireRentSource(source, sourceField);

  const double base = source();
  requireRentAmount(base, sourceField);

  const double multiplier = growth::sampleLognormalMultiplier(rng, 0.05);
  const double amount = base * multiplier;

  requireRentAmount(amount, "baseRent");
  return amount;
}

} // namespace internal

/// Bootstrap the initial lease state for a payer account.
[[nodiscard]] inline Lease initializeLease(const LeaseRules &rules,
                                           const random::RngFactory &factory,
                                           random::Rng &rng,
                                           const LeaseInitInput &input) {
  primitives::validate::require(rules);
  internal::requireKey(input.payerKey, "payerKey");

  auto initRng = factory.rng({"lease_init", input.payerKey});

  const auto landlord = growth::pickOne(initRng, input.landlords);

  const auto interval =
      growth::sampleBackdatedInterval(initRng, input.startDate, rules.tenure);

  const double baseRent =
      internal::sampleBaseRent(initRng, input.rentSource, "rentSource");

  // Preserve the caller-visible RNG stream movement from the previous version.
  (void)rng.nextDouble();

  return Lease{
      .landlordAcct = landlord,
      .start = interval.start,
      .end = interval.end,
      .baseRent = baseRent,
      .moveIndex = 0,
  };
}

/// Transition to a new lease with fresh landlord and base rent.
[[nodiscard]] inline Lease advanceLease(const LeaseRules &rules,
                                        const random::RngFactory &factory,
                                        random::Rng &rng,
                                        const LeaseAdvanceInput &input) {
  primitives::validate::require(rules);
  primitives::validate::require(input.previous);
  internal::requireKey(input.payerKey, "payerKey");

  const auto landlord =
      growth::pickDifferent(rng, input.landlords, input.previous.landlordAcct);

  const auto interval =
      growth::sampleForwardInterval(rng, input.now, rules.tenure);

  auto resetRng = factory.rng({"lease_reset_rent", input.payerKey,
                               std::to_string(input.previous.moveIndex + 1)});

  const double baseRent = internal::sampleBaseRent(
      resetRng, input.resetRentSource, "resetRentSource");

  return Lease{
      .landlordAcct = landlord,
      .start = interval.start,
      .end = interval.end,
      .baseRent = baseRent,
      .moveIndex = input.previous.moveIndex + 1,
  };
}

/// Calculate the rent for a specific month, including compounded growth.
[[nodiscard]] inline double calculateRent(const LeaseRules &rules,
                                          const random::RngFactory &factory,
                                          const RentQuery &query) {
  primitives::validate::require(rules);
  primitives::validate::require(query.state);
  internal::requireKey(query.payerKey, "payerKey");

  const growth::AnnualRaiseSeries raises{factory, "rent_real_raise",
                                         query.payerKey};

  const double amount =
      query.state.baseRent * growth::compoundGrowth(rules.growth.annual, raises,
                                                    query.state.start,
                                                    query.payDate);

  primitives::validate::finite("rentAmount", amount);

  return primitives::utils::roundMoney(std::max(1.0, amount));
}

} // namespace PhantomLedger::activity::recurring
