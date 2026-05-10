#pragma once

#include "phantomledger/entities/products/insurance_ledger.hpp"
#include "phantomledger/entities/products/loan_terms_ledger.hpp"
#include "phantomledger/entities/products/obligation_stream.hpp"

namespace PhantomLedger::entity::product {

class PortfolioRegistry {
public:
  PortfolioRegistry() = default;

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

private:
  InsuranceLedger insurance_;
  LoanTermsLedger loans_;
  ObligationStream obligations_;
};

} // namespace PhantomLedger::entity::product
