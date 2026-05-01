#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/taxonomies/products/types.hpp"

#include <cstdint>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>

namespace PhantomLedger::entity::product {

using ::PhantomLedger::products::PolicyType;

/// A single active insurance policy per coverage type.
struct InsurancePolicy {
  PolicyType policyType = PolicyType::auto_;
  entity::Key carrierAcct{};
  double monthlyPremium = 0.0;
  std::int32_t billingDay = 1;
  double annualClaimP = 0.0;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;

    r.check([&] { v::positive("monthlyPremium", monthlyPremium); });
    r.check([&] { v::between("billingDay", billingDay, 1, 28); });
    r.check([&] { v::unit("annualClaimP", annualClaimP); });
  }
};

class InsuranceHoldings {
public:
  InsuranceHoldings() = default;

  InsuranceHoldings(std::optional<InsurancePolicy> autoPolicy,
                    std::optional<InsurancePolicy> homePolicy,
                    std::optional<InsurancePolicy> lifePolicy)
      : autoPolicy_(std::move(autoPolicy)), homePolicy_(std::move(homePolicy)),
        lifePolicy_(std::move(lifePolicy)) {}

  [[nodiscard]] const std::optional<InsurancePolicy> &
  autoPolicy() const noexcept {
    return autoPolicy_;
  }

  [[nodiscard]] const std::optional<InsurancePolicy> &
  homePolicy() const noexcept {
    return homePolicy_;
  }

  [[nodiscard]] const std::optional<InsurancePolicy> &
  lifePolicy() const noexcept {
    return lifePolicy_;
  }

  [[nodiscard]] const InsurancePolicy *get(PolicyType type) const noexcept {
    switch (type) {
    case PolicyType::auto_:
      return autoPolicy_ ? &*autoPolicy_ : nullptr;

    case PolicyType::home:
      return homePolicy_ ? &*homePolicy_ : nullptr;

    case PolicyType::life:
      return lifePolicy_ ? &*lifePolicy_ : nullptr;
    }

    std::unreachable();
  }

  [[nodiscard]] bool has(PolicyType type) const noexcept {
    return get(type) != nullptr;
  }

  template <class F> void forEach(F &&visit) const {
    if (autoPolicy_) {
      visit(*autoPolicy_);
    }
    if (homePolicy_) {
      visit(*homePolicy_);
    }
    if (lifePolicy_) {
      visit(*lifePolicy_);
    }
  }

  void validate(primitives::validate::Report &r) const {
    validateSlot(r, "autoPolicy", autoPolicy_, PolicyType::auto_);
    validateSlot(r, "homePolicy", homePolicy_, PolicyType::home);
    validateSlot(r, "lifePolicy", lifePolicy_, PolicyType::life);
  }

  [[nodiscard]] int activeCount() const noexcept {
    return (autoPolicy_.has_value() ? 1 : 0) +
           (homePolicy_.has_value() ? 1 : 0) +
           (lifePolicy_.has_value() ? 1 : 0);
  }

  [[nodiscard]] double totalMonthlyPremium() const noexcept {
    double total = 0.0;

    forEach(
        [&](const InsurancePolicy &policy) { total += policy.monthlyPremium; });

    return total;
  }

  [[nodiscard]] bool hasAny() const noexcept { return activeCount() > 0; }

private:
  static void validateSlot(primitives::validate::Report &r,
                           std::string_view field,
                           const std::optional<InsurancePolicy> &policy,
                           PolicyType expected) {
    if (!policy.has_value()) {
      return;
    }

    policy->validate(r);

    r.check([&] {
      if (policy->policyType != expected) {
        std::string message;
        message.reserve(field.size() + 32);
        message.append(field);
        message.append(" has mismatched policyType");

        throw primitives::validate::Error(std::move(message),
                                          std::source_location::current());
      }
    });
  }

  std::optional<InsurancePolicy> autoPolicy_;
  std::optional<InsurancePolicy> homePolicy_;
  std::optional<InsurancePolicy> lifePolicy_;
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
