#pragma once
/*
 * InsuranceLedger — per-person insurance holdings.
 *
 * One of the three concerns split out of the old PortfolioRegistry.
 * This class is responsible for storing which insurance policies a
 * person holds and for iterating them. It knows nothing about loans
 * or scheduled obligation events.
 */

#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/entities/products/insurance.hpp"

#include <unordered_map>
#include <utility>

namespace PhantomLedger::entity::product {

class InsuranceLedger {
public:
  InsuranceLedger() = default;

  void set(entity::PersonId person, InsuranceHoldings holdings) {
    byPerson_.insert_or_assign(person, std::move(holdings));
  }

  [[nodiscard]] const InsuranceHoldings *
  get(entity::PersonId person) const noexcept {
    const auto it = byPerson_.find(person);
    return it == byPerson_.end() ? nullptr : &it->second;
  }

  /// Visit every insured person in arbitrary order. `visit` is called
  /// with `(PersonId, const InsuranceHoldings&)`.
  template <typename F> void forEach(F &&visit) const {
    for (const auto &[person, holdings] : byPerson_) {
      visit(person, holdings);
    }
  }

  [[nodiscard]] std::size_t size() const noexcept { return byPerson_.size(); }

private:
  std::unordered_map<entity::PersonId, InsuranceHoldings> byPerson_;
};

} // namespace PhantomLedger::entity::product
