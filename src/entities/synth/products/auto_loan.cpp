#include "phantomledger/entities/synth/products/auto_loan.hpp"

#include "phantomledger/entities/synth/products/amount_sampling.hpp"
#include "phantomledger/entities/synth/products/dates.hpp"
#include "phantomledger/entities/synth/products/installment_emission.hpp"
#include "phantomledger/entities/synth/products/institutional.hpp"
#include "phantomledger/probability/distributions/normal.hpp"

#include <algorithm>
#include <cmath>

namespace PhantomLedger::entities::synth::products {

namespace {

namespace product = ::PhantomLedger::entity::product;

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

[[nodiscard]] bool
emitAutoLoan(::PhantomLedger::random::Rng &rng,
             ::PhantomLedger::entity::product::PortfolioRegistry &portfolios,
             ::PhantomLedger::entity::PersonId person, personaTax::Type persona,
             ::PhantomLedger::time::Window window, const AutoLoanTerms &terms) {
  if (rng.nextDouble() >= terms.adoption.probability(persona)) {
    return false;
  }

  const bool isNew = rng.nextDouble() < terms.vehicleMix.newVehicleShare;

  const double median =
      isNew ? terms.payment.newMedian : terms.payment.usedMedian;
  const double sigma = isNew ? terms.payment.newSigma : terms.payment.usedSigma;
  const double payment =
      samplePaymentAmount(rng, median, sigma, terms.payment.floor);

  const std::int32_t termMonths = sampleAutoTermMonths(rng, terms.term, isNew);

  const std::int32_t maxAgeDays = std::max<std::int32_t>(30, termMonths * 30);
  const std::int32_t ageDays = static_cast<std::int32_t>(
      rng.uniformInt(30, static_cast<std::int64_t>(maxAgeDays) + 1));

  const auto loanStart = window.start - ::PhantomLedger::time::Days{ageDays};

  addInstallmentProduct(portfolios, person, product::ProductType::autoLoan,
                        institutional::autoLender(), loanStart, termMonths,
                        samplePaymentDay(rng), payment, window,
                        delinquencyKnobs(terms.delinquency));

  return true;
}

} // namespace PhantomLedger::entities::synth::products
