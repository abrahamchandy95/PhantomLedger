#include "phantomledger/entities/synth/products/mortgage.hpp"

#include "phantomledger/entities/synth/products/amount_sampling.hpp"
#include "phantomledger/entities/synth/products/installment_emission.hpp"
#include "phantomledger/entities/synth/products/institutional.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace PhantomLedger::entities::synth::products {

namespace {

namespace product = ::PhantomLedger::entity::product;

[[nodiscard]] double triangular(::PhantomLedger::random::Rng &rng, double left,
                                double mode, double right) {
  const double u = rng.nextDouble();
  const double fc = (mode - left) / (right - left);

  if (u < fc) {
    return left + std::sqrt(u * (right - left) * (mode - left));
  }

  return right - std::sqrt((1.0 - u) * (right - left) * (right - mode));
}

[[nodiscard]] std::int32_t
sampleMortgagePaymentDay(::PhantomLedger::random::Rng &rng) {
  if (rng.coin(0.85)) {
    return 1;
  }

  return static_cast<std::int32_t>(rng.uniformInt(2, 6));
}

[[nodiscard]] std::int32_t
sampleMortgageAgeDays(::PhantomLedger::random::Rng &rng) {
  const double rawYears = triangular(rng, 0.5, 5.0, 10.0);
  const auto days = static_cast<std::int32_t>(std::round(rawYears * 365.0));

  return std::max<std::int32_t>(30, days);
}

} // namespace

MortgageEmitter::MortgageEmitter(::PhantomLedger::random::Rng &rng,
                                 ::PhantomLedger::time::Window window,
                                 MortgageTerms terms)
    : rng_{&rng}, window_{window}, terms_{std::move(terms)} {}

[[nodiscard]] bool MortgageEmitter::emit(
    ::PhantomLedger::entity::PersonId person, personaTax::Type persona,
    ::PhantomLedger::entity::product::LoanTermsLedger &loans,
    ::PhantomLedger::entity::product::ObligationStream &obligations) {
  if (rng_->nextDouble() >= terms_.adoption.probability(persona)) {
    return false;
  }

  const double payment = samplePaymentAmount(*rng_, terms_.payment.median,
                                             terms_.payment.sigma, 200.0);

  const std::int32_t paymentDay = sampleMortgagePaymentDay(*rng_);
  const std::int32_t ageDays = sampleMortgageAgeDays(*rng_);
  const auto loanStart = window_.start - ::PhantomLedger::time::Days{ageDays};

  constexpr std::int32_t kMortgageTermMonths = 360;

  addInstallmentProduct(loans, obligations, window_,
                        InstallmentIssue{
                            .person = person,
                            .productType = product::ProductType::mortgage,
                            .counterparty = institutional::mortgageLender(),
                            .start = loanStart,
                            .termMonths = kMortgageTermMonths,
                            .paymentDay = paymentDay,
                            .monthlyPayment = payment,
                            .terms = installmentTerms(terms_.delinquency),
                        });

  return true;
}

} // namespace PhantomLedger::entities::synth::products
