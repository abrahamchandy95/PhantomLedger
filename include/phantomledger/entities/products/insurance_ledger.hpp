#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/products/insurance.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

#include <unordered_map>
#include <utility>

namespace PhantomLedger::entity::product {

class InsuranceLedger {
public:
  InsuranceLedger() = default;

  void set(entity::PersonId person, InsuranceHoldings holdings) {
    primitives::validate::require(holdings);
    byPerson_.insert_or_assign(person, std::move(holdings));
  }

  [[nodiscard]] const InsuranceHoldings *
  get(entity::PersonId person) const noexcept {
    const auto it = byPerson_.find(person);
    return it == byPerson_.end() ? nullptr : &it->second;
  }

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
