#pragma once
/*
 * PortfolioRegistry — thin facade that composes three concerns:
 *
 *   - InsuranceLedger   (insurance holdings per person)
 *   - LoanTermsLedger   (installment terms per (person, product))
 *   - ObligationStream  (time-sorted event stream)
 *
 * Kept as a single type so existing call sites that take
 * `const PortfolioRegistry&` still compile — but internally each
 * operation lives on the piece that owns it. Transfer generators
 * that only touch one concern should prefer to take the narrower
 * type.
 */

#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/entities/products/event.hpp"
#include "phantomledger/entities/products/insurance_ledger.hpp"
#include "phantomledger/entities/products/loan_terms_ledger.hpp"
#include "phantomledger/entities/products/obligation_stream.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

#include <span>
#include <utility>

namespace PhantomLedger::entity::product {

class PortfolioRegistry {
public:
  PortfolioRegistry() = default;

  // --- Component access ---
  //
  // Prefer these when a caller only needs one concern.

  [[nodiscard]] InsuranceLedger &insurance() noexcept { return insurance_; }
  [[nodiscard]] const InsuranceLedger &insurance() const noexcept {
    return insurance_;
  }

  [[nodiscard]] LoanTermsLedger &loans() noexcept { return loans_; }
  [[nodiscard]] const LoanTermsLedger &loans() const noexcept { return loans_; }

  [[nodiscard]] ObligationStream &obligations() noexcept {
    return obligations_;
  }
  [[nodiscard]] const ObligationStream &obligations() const noexcept {
    return obligations_;
  }

  // --- Convenience passthroughs ---
  //
  // Kept tight: just enough so the existing transfer generators
  // don't have to rewrite at call sites. New code should reach
  // through .insurance() / .loans() / .obligations() directly.

  void setInsurance(entity::PersonId person, InsuranceHoldings holdings) {
    insurance_.set(person, std::move(holdings));
  }

  [[nodiscard]] const InsuranceHoldings *
  insurance(entity::PersonId person) const noexcept {
    return insurance_.get(person);
  }

  template <typename F> void forEachInsuredPerson(F &&visit) const {
    insurance_.forEach(std::forward<F>(visit));
  }

  void setInstallmentTerms(entity::PersonId person, ProductType productType,
                           InstallmentTerms terms) {
    loans_.set(person, productType, terms);
  }

  [[nodiscard]] const InstallmentTerms *
  installmentTerms(entity::PersonId person,
                   ProductType productType) const noexcept {
    return loans_.get(person, productType);
  }

  [[nodiscard]] bool hasMortgage(entity::PersonId person) const noexcept {
    return loans_.hasMortgage(person);
  }

  void addEvent(ObligationEvent event) {
    obligations_.append(std::move(event));
  }

  void sortEvents() { obligations_.sort(); }

  [[nodiscard]] std::span<const ObligationEvent>
  allEvents(time::TimePoint start, time::TimePoint endExcl) const noexcept {
    return obligations_.between(start, endExcl);
  }

private:
  InsuranceLedger insurance_;
  LoanTermsLedger loans_;
  ObligationStream obligations_;
};

} // namespace PhantomLedger::entity::product
