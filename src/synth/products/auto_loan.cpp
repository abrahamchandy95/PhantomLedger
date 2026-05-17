#include "phantomledger/synth/products/auto_loan.hpp"

#include "phantomledger/primitives/random/distributions/normal.hpp"
#include "phantomledger/synth/products/amount_sampling.hpp"
#include "phantomledger/synth/products/dates.hpp"
#include "phantomledger/synth/products/installment_emission.hpp"
#include "phantomledger/taxonomies/counterparties/accounts.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace PhantomLedger::synth::products {

namespace {

namespace product = ::PhantomLedger::entity::product;
namespace counterparties = ::PhantomLedger::counterparties;

[[nodiscard]] std::int32_t
sampleAutoTermMonths(::PhantomLedger::random::Rng &rng,
                     const AutoLoanTerm &term, bool isNew) {
  const double mean = isNew ? term.newMeanMonths : term.usedMeanMonths;
  const double sigma = isNew ? term.newSigmaMonths : term.usedSigmaMonths;

  const double raw =
      ::PhantomLedger::probability::distributions::normal(rng, mean, sigma);
  const auto months = static_cast<std::int32_t>(std::round(raw));

  return std::clamp(months, term.minMonths, term.maxMonths);
}

} // namespace

AutoLoanEmitter::AutoLoanEmitter(::PhantomLedger::random::Rng &rng,
                                 ::PhantomLedger::time::Window window,
                                 AutoLoanTerms terms)
    : rng_{&rng}, window_{window}, terms_{std::move(terms)} {}

[[nodiscard]] bool AutoLoanEmitter::emit(
    ::PhantomLedger::entity::PersonId person, personaTax::Type persona,
    ::PhantomLedger::entity::product::LoanTermsLedger &loans,
    ::PhantomLedger::entity::product::ObligationStream &obligations) {
  if (rng_->nextDouble() >= terms_.adoption.probability(persona)) {
    return false;
  }

  const bool isNew = rng_->nextDouble() < terms_.vehicleMix.newVehicleShare;

  const double median =
      isNew ? terms_.payment.newMedian : terms_.payment.usedMedian;
  const double sigma =
      isNew ? terms_.payment.newSigma : terms_.payment.usedSigma;
  const double payment =
      samplePaymentAmount(*rng_, median, sigma, terms_.payment.floor);

  const std::int32_t termMonths =
      sampleAutoTermMonths(*rng_, terms_.term, isNew);

  const std::int32_t maxAgeDays = std::max<std::int32_t>(30, termMonths * 30);
  const std::int32_t ageDays = static_cast<std::int32_t>(
      rng_->uniformInt(30, static_cast<std::int64_t>(maxAgeDays) + 1));

  const auto loanStart = window_.start - ::PhantomLedger::time::Days{ageDays};

  addInstallmentProduct(loans, obligations, window_,
                        InstallmentIssue{
                            .person = person,
                            .productType = product::ProductType::autoLoan,
                            .counterparty = counterparties::key(
                                counterparties::Lending::autoLoan),
                            .start = loanStart,
                            .termMonths = termMonths,
                            .paymentDay = samplePaymentDay(*rng_),
                            .monthlyPayment = payment,
                            .terms = installmentTerms(terms_.delinquency),
                        });

  return true;
}

} // namespace PhantomLedger::synth::products
