#include "phantomledger/entities/synth/products/insurance.hpp"

#include "phantomledger/entities/products/insurance.hpp"
#include "phantomledger/entities/synth/products/amount_sampling.hpp"
#include "phantomledger/entities/synth/products/dates.hpp"
#include "phantomledger/entities/synth/products/institutional.hpp"

#include <algorithm>
#include <optional>
#include <utility>

namespace PhantomLedger::entities::synth::products {

namespace {

namespace product = ::PhantomLedger::entity::product;

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

[[nodiscard]] bool
emitInsurance(::PhantomLedger::random::Rng &rng,
              ::PhantomLedger::entity::product::PortfolioRegistry &portfolios,
              ::PhantomLedger::entity::PersonId person,
              personaTax::Type persona, bool hasMortgage, bool hasAutoLoan,
              double mortgageAnchorP, double autoLoanAnchorP,
              const InsuranceTerms &terms) {
  const double autoAnchorPolicyP = terms.loanRequirements.autoLoanRequiresAutoP;
  const double autoIssueP =
      hasAutoLoan
          ? autoAnchorPolicyP
          : residualPolicyProbability(terms.adoption.autoProbability(persona),
                                      autoLoanAnchorP, autoAnchorPolicyP);

  std::optional<product::InsurancePolicy> autoPol;
  if (rng.nextDouble() < autoIssueP) {
    const double premium = samplePaymentAmount(
        rng, terms.premiums.autoPolicy.median, terms.premiums.autoPolicy.sigma,
        terms.premiums.autoPolicy.floor);
    autoPol =
        product::autoPolicy(institutional::autoCarrier(), premium,
                            samplePaymentDay(rng), terms.claims.autoAnnualP);
  }

  const double homeAnchorPolicyP = terms.loanRequirements.mortgageRequiresHomeP;
  const double homeIssueP =
      hasMortgage
          ? homeAnchorPolicyP
          : residualPolicyProbability(terms.adoption.homeProbability(persona),
                                      mortgageAnchorP, homeAnchorPolicyP);

  std::optional<product::InsurancePolicy> homePol;
  if (rng.nextDouble() < homeIssueP) {
    const double premium = samplePaymentAmount(
        rng, terms.premiums.homePolicy.median, terms.premiums.homePolicy.sigma,
        terms.premiums.homePolicy.floor);
    homePol =
        product::homePolicy(institutional::homeCarrier(), premium,
                            samplePaymentDay(rng), terms.claims.homeAnnualP);
  }

  std::optional<product::InsurancePolicy> lifePol;
  if (rng.nextDouble() < terms.adoption.lifeProbability(persona)) {
    const double premium = samplePaymentAmount(
        rng, terms.premiums.lifePolicy.median, terms.premiums.lifePolicy.sigma,
        terms.premiums.lifePolicy.floor);
    lifePol = product::lifePolicy(institutional::lifeCarrier(), premium,
                                  samplePaymentDay(rng));
  }

  if (!autoPol.has_value() && !homePol.has_value() && !lifePol.has_value()) {
    return false;
  }

  portfolios.insurance().set(person, product::InsuranceHoldings{
                                         std::move(autoPol),
                                         std::move(homePol),
                                         std::move(lifePol),
                                     });

  return true;
}

} // namespace PhantomLedger::entities::synth::products
