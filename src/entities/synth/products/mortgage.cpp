#include "phantomledger/entities/synth/products/mortgage.hpp"

#include "phantomledger/entities/synth/products/amount_sampling.hpp"
#include "phantomledger/entities/synth/products/installment_emission.hpp"
#include "phantomledger/entities/synth/products/institutional.hpp"

#include <algorithm>
#include <cmath>

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

[[nodiscard]] bool
emitMortgage(::PhantomLedger::random::Rng &rng,
             ::PhantomLedger::entity::product::PortfolioRegistry &portfolios,
             ::PhantomLedger::entity::PersonId person, personaTax::Type persona,
             ::PhantomLedger::time::Window window, const MortgageTerms &terms) {
  if (rng.nextDouble() >= terms.adoption.probability(persona)) {
    return false;
  }

  const double payment = samplePaymentAmount(rng, terms.payment.median,
                                             terms.payment.sigma, 200.0);

  const std::int32_t paymentDay = sampleMortgagePaymentDay(rng);
  const std::int32_t ageDays = sampleMortgageAgeDays(rng);
  const auto loanStart = window.start - ::PhantomLedger::time::Days{ageDays};

  constexpr std::int32_t kMortgageTermMonths = 360;

  addInstallmentProduct(portfolios, person, product::ProductType::mortgage,
                        institutional::mortgageLender(), loanStart,
                        kMortgageTermMonths, paymentDay, payment, window,
                        delinquencyKnobs(terms.delinquency));

  return true;
}

} // namespace PhantomLedger::entities::synth::products
