#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/products/installment_terms.hpp"
#include "phantomledger/entities/products/loan_terms_ledger.hpp"
#include "phantomledger/entities/products/obligation_stream.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"

#include <cstdint>

namespace PhantomLedger::synth::products {

template <class Delinquency>
[[nodiscard]] constexpr ::PhantomLedger::entity::product::InstallmentTerms
installmentTerms(const Delinquency &d) noexcept {
  return {
      .lateP = d.lateP,
      .lateDaysMin = d.lateDaysMin,
      .lateDaysMax = d.lateDaysMax,
      .missP = d.missP,
      .partialP = d.partialP,
      .cureP = d.cureP,
      .partialMinFrac = d.partialMinFrac,
      .partialMaxFrac = d.partialMaxFrac,
      .clusterMult = d.clusterMult,
  };
}

struct InstallmentIssue {
  ::PhantomLedger::entity::PersonId person{};
  ::PhantomLedger::entity::product::ProductType productType =
      ::PhantomLedger::entity::product::ProductType::unknown;
  ::PhantomLedger::entity::Key counterparty{};
  ::PhantomLedger::time::TimePoint start{};
  std::int32_t termMonths = 0;
  std::int32_t paymentDay = 1;
  double monthlyPayment = 0.0;
  ::PhantomLedger::entity::product::InstallmentTerms terms{};
};

void addInstallmentProduct(
    ::PhantomLedger::entity::product::LoanTermsLedger &loans,
    ::PhantomLedger::entity::product::ObligationStream &obligations,
    ::PhantomLedger::time::Window window, const InstallmentIssue &issue);

} // namespace PhantomLedger::synth::products
