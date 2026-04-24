#pragma once
/*
 * LoanTermsLedger — installment-loan behavior terms per (person,
 * productType).
 *
 * Replaces two concerns from the old PortfolioRegistry:
 *
 *   1. installmentByKey_                — stored here
 *   2. hasMortgage_ (std::unordered_set) — derived here
 *
 * The separate hasMortgage_ set was redundant — a mortgage exists iff
 * there are installment terms under (person, mortgage). This keeps
 * one source of truth.
 */

#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/entities/products/event.hpp"
#include "phantomledger/entities/products/installment_terms.hpp"

#include <cassert>
#include <cstdint>
#include <functional>
#include <unordered_map>

namespace PhantomLedger::entity::product {

namespace detail {

struct PersonProductKey {
  entity::PersonId personId{};
  ProductType productType = ProductType::unknown;

  [[nodiscard]] constexpr bool
  operator==(const PersonProductKey &) const noexcept = default;
};

struct PersonProductKeyHash {
  [[nodiscard]] std::size_t
  operator()(const PersonProductKey &k) const noexcept {
    const auto pid = static_cast<std::uint64_t>(k.personId);
    const auto pt = static_cast<std::uint64_t>(k.productType);
    return std::hash<std::uint64_t>{}((pid << 8U) ^ pt);
  }
};

} // namespace detail

class LoanTermsLedger {
public:
  LoanTermsLedger() = default;

  void set(entity::PersonId person, ProductType productType,
           InstallmentTerms terms) {
    assert(isInstallmentLoan(productType));
    byKey_.insert_or_assign({person, productType}, terms);
  }

  [[nodiscard]] const InstallmentTerms *
  get(entity::PersonId person, ProductType productType) const noexcept {
    const auto it = byKey_.find({person, productType});
    return it == byKey_.end() ? nullptr : &it->second;
  }

  /// Derived flag: true iff the person has an active mortgage term
  /// registered. The insurance generator checks this to avoid
  /// double-emitting home-insurance premiums that are already
  /// escrowed into the mortgage payment.
  [[nodiscard]] bool hasMortgage(entity::PersonId person) const noexcept {
    return byKey_.contains({person, ProductType::mortgage});
  }

  [[nodiscard]] std::size_t size() const noexcept { return byKey_.size(); }

private:
  std::unordered_map<detail::PersonProductKey, InstallmentTerms,
                     detail::PersonProductKeyHash>
      byKey_;
};

} // namespace PhantomLedger::entity::product
