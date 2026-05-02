#pragma once

#include "phantomledger/taxonomies/personas/table.hpp"

namespace PhantomLedger::entities::synth::products {

namespace personaTax = ::PhantomLedger::personas;

struct TaxObligationAdoption {
  personaTax::Rates ownership{
      .student = 0.05,
      .retiree = 0.20,
      .freelancer = 0.65,
      .smallBusiness = 0.85,
      .highNetWorth = 0.50,
      .salaried = 0.10,
  };

  [[nodiscard]] constexpr double
  probability(personaTax::Type type) const noexcept {
    return ownership.get(type);
  }
};

struct QuarterlyTaxPayment {
  double median = 1250.0;
  double sigma = 0.65;
  double floor = 100.0;
};

struct TaxFilingOutcome {
  double refundP = 0.65;
  double balanceDueP = 0.20;
};

struct TaxRefund {
  double median = 1850.0;
  double sigma = 0.55;
  double floor = 100.0;
};

struct TaxBalanceDue {
  double median = 1100.0;
  double sigma = 0.65;
  double floor = 100.0;
};

struct TaxTerms {
  TaxObligationAdoption adoption{};
  QuarterlyTaxPayment quarterlyPayment{};
  TaxFilingOutcome filingOutcome{};
  TaxRefund refund{};
  TaxBalanceDue balanceDue{};
};

} // namespace PhantomLedger::entities::synth::products
