#include "phantomledger/synth/products/terms/insurance.hpp"

#include "phantomledger/entities/products/insurance.hpp"
#include "phantomledger/synth/products/sampling/amounts.hpp"
#include "phantomledger/synth/products/sampling/dates.hpp"
#include "phantomledger/taxonomies/counterparties/accounts.hpp"

#include <algorithm>
#include <optional>
#include <utility>

namespace PhantomLedger::synth::products {

namespace {

namespace product = ::PhantomLedger::entity::product;
namespace counterparties = ::PhantomLedger::counterparties;

[[nodiscard]] constexpr double clamp01(double v) noexcept {
  return std::max(0.0, std::min(1.0, v));
}

[[nodiscard]] double residualPolicyProbability(double targetP,
                                               double anchoredPopulationP,
                                               double anchoredPolicyP) {
  const double target = clamp01(targetP);
  const double anchoredShare = clamp01(anchoredPopulationP);
  const double anchoredPolicy = clamp01(anchoredPolicyP);

  if (target <= 0.0) {
    return 0.0;
  }

  if (anchoredShare >= 1.0) {
    return anchoredPolicy;
  }

  const double residual =
      (target - anchoredShare * anchoredPolicy) / (1.0 - anchoredShare);

  return clamp01(residual);
}

} // namespace

InsuranceEmitter::InsuranceEmitter(
    ::PhantomLedger::random::Rng &rng,
    ::PhantomLedger::entity::product::InsuranceLedger &insurance,
    InsuranceTerms terms)
    : rng_{&rng}, insurance_{&insurance}, terms_{std::move(terms)} {}

[[nodiscard]] bool
InsuranceEmitter::emit(::PhantomLedger::entity::PersonId person,
                       personaTax::Type persona, LoanAnchors anchors) {
  const double autoAnchorPolicyP =
      terms_.loanRequirements.autoLoanRequiresAutoP;
  const double autoIssueP =
      anchors.hasAutoLoan
          ? autoAnchorPolicyP
          : residualPolicyProbability(terms_.adoption.autoProbability(persona),
                                      anchors.autoLoanP, autoAnchorPolicyP);

  std::optional<product::InsurancePolicy> autoPol;
  if (rng_->nextDouble() < autoIssueP) {
    const double premium = samplePaymentAmount(
        *rng_, terms_.premiums.autoPolicy.median,
        terms_.premiums.autoPolicy.sigma, terms_.premiums.autoPolicy.floor);
    autoPol = product::autoPolicy(
        counterparties::key(counterparties::Insurance::autoCarrier), premium,
        samplePaymentDay(*rng_), terms_.claims.autoAnnualP);
  }

  const double homeAnchorPolicyP =
      terms_.loanRequirements.mortgageRequiresHomeP;
  const double homeIssueP =
      anchors.hasMortgage
          ? homeAnchorPolicyP
          : residualPolicyProbability(terms_.adoption.homeProbability(persona),
                                      anchors.mortgageP, homeAnchorPolicyP);

  std::optional<product::InsurancePolicy> homePol;
  if (rng_->nextDouble() < homeIssueP) {
    const double premium = samplePaymentAmount(
        *rng_, terms_.premiums.homePolicy.median,
        terms_.premiums.homePolicy.sigma, terms_.premiums.homePolicy.floor);
    homePol = product::homePolicy(
        counterparties::key(counterparties::Insurance::homeCarrier), premium,
        samplePaymentDay(*rng_), terms_.claims.homeAnnualP);
  }

  std::optional<product::InsurancePolicy> lifePol;
  if (rng_->nextDouble() < terms_.adoption.lifeProbability(persona)) {
    const double premium = samplePaymentAmount(
        *rng_, terms_.premiums.lifePolicy.median,
        terms_.premiums.lifePolicy.sigma, terms_.premiums.lifePolicy.floor);
    lifePol = product::lifePolicy(
        counterparties::key(counterparties::Insurance::lifeCarrier), premium,
        samplePaymentDay(*rng_));
  }

  if (!autoPol.has_value() && !homePol.has_value() && !lifePol.has_value()) {
    return false;
  }

  insurance_->set(person, product::InsuranceHoldings{
                              std::move(autoPol),
                              std::move(homePol),
                              std::move(lifePol),
                          });

  return true;
}

} // namespace PhantomLedger::synth::products
