#include "phantomledger/synth/products/terms/mortgage.hpp"

#include "phantomledger/synth/econ/nominal.hpp"
#include "phantomledger/synth/products/installments.hpp"
#include "phantomledger/synth/products/sampling/amounts.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace PhantomLedger::synth::products {

namespace {

namespace product = ::PhantomLedger::entity::product;
namespace counterparties = ::PhantomLedger::counterparties;

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
                                 const ProviderPicker &providers,
                                 MortgageTerms terms)
    : rng_{&rng}, window_{window}, providers_{&providers},
      terms_{std::move(terms)} {}

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

  // H1 step 2b (class D): real loans are nominal contracts — the
  // payment anchors at the ORIGINATION year's price level and stays
  // fixed nominal through the term. Pre-1990 originations clamp to
  // coverage (freeze-and-declare via scaleYear; authority U-6).
  const double nominalPayment =
      payment * ::PhantomLedger::synth::econ::priceScale(
                    ::PhantomLedger::time::toCalendarDate(loanStart).year);

  // The servicer comes from the mortgage market on its own lane
  // (providers.hpp), so it spends nothing from rng_. Until
  // institutional-providers-2026-09 every mortgage paid the student-loan
  // servicer; test_product_providers carries the predicate that keeps it
  // closed.
  addInstallmentProduct(
      loans, obligations, window_,
      InstallmentIssue{
          .person = person,
          .productType = product::ProductType::mortgage,
          .counterparty = providers_->pick(counterparties::Market::mortgage),
          .start = loanStart,
          .termMonths = kMortgageTermMonths,
          .paymentDay = paymentDay,
          .monthlyPayment = nominalPayment,
          .terms = installmentTerms(terms_.delinquency),
      });

  return true;
}

} // namespace PhantomLedger::synth::products
