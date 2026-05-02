#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/products/portfolio.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/taxonomies/personas/table.hpp"

namespace PhantomLedger::entities::synth::products {

namespace personaTax = ::PhantomLedger::personas;

struct InsuranceAdoption {
  personaTax::Rates autoPolicy{
      .student = 0.30,
      .retiree = 0.85,
      .freelancer = 0.85,
      .smallBusiness = 0.90,
      .highNetWorth = 0.95,
      .salaried = 0.92,
  };

  personaTax::Rates homePolicy{
      .student = 0.05,
      .retiree = 0.55,
      .freelancer = 0.30,
      .smallBusiness = 0.55,
      .highNetWorth = 0.70,
      .salaried = 0.55,
  };

  personaTax::Rates lifePolicy{
      .student = 0.10,
      .retiree = 0.55,
      .freelancer = 0.30,
      .smallBusiness = 0.45,
      .highNetWorth = 0.55,
      .salaried = 0.55,
  };

  [[nodiscard]] constexpr double
  autoProbability(personaTax::Type type) const noexcept {
    return autoPolicy.get(type);
  }

  [[nodiscard]] constexpr double
  homeProbability(personaTax::Type type) const noexcept {
    return homePolicy.get(type);
  }

  [[nodiscard]] constexpr double
  lifeProbability(personaTax::Type type) const noexcept {
    return lifePolicy.get(type);
  }
};

struct InsuranceLoanRequirements {
  double mortgageRequiresHomeP = 0.99;
  double autoLoanRequiresAutoP = 0.997;
};

struct InsurancePremium {
  double median = 0.0;
  double sigma = 0.0;
  double floor = 5.0;
};

struct InsurancePremiums {
  InsurancePremium autoPolicy{
      .median = 225.0,
      .sigma = 0.30,
      .floor = 25.0,
  };

  InsurancePremium homePolicy{
      .median = 163.0,
      .sigma = 0.30,
      .floor = 25.0,
  };

  InsurancePremium lifePolicy{
      .median = 28.0,
      .sigma = 0.40,
      .floor = 5.0,
  };
};

struct InsuranceClaims {
  double autoAnnualP = 0.042;
  double homeAnnualP = 0.055;
};

struct InsuranceTerms {
  InsuranceAdoption adoption{};
  InsuranceLoanRequirements loanRequirements{};
  InsurancePremiums premiums{};
  InsuranceClaims claims{};
};

[[nodiscard]] bool
emitInsurance(::PhantomLedger::random::Rng &rng,
              ::PhantomLedger::entity::product::PortfolioRegistry &portfolios,
              ::PhantomLedger::entity::PersonId person,
              personaTax::Type persona, bool hasMortgage, bool hasAutoLoan,
              double mortgageAnchorP, double autoLoanAnchorP,
              const InsuranceTerms &terms = {});

} // namespace PhantomLedger::entities::synth::products
