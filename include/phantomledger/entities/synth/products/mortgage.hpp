#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/products/portfolio.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/taxonomies/personas/table.hpp"

#include <cstdint>

namespace PhantomLedger::entities::synth::products {

namespace personaTax = ::PhantomLedger::personas;

struct MortgageAdoption {
  personaTax::Rates ownership{
      .student = 0.02,
      .retiree = 0.55,
      .freelancer = 0.30,
      .smallBusiness = 0.55,
      .highNetWorth = 0.65,
      .salaried = 0.55,
  };

  [[nodiscard]] constexpr double
  probability(personaTax::Type type) const noexcept {
    return ownership.get(type);
  }
};

struct MortgagePayment {
  double median = 1750.0;
  double sigma = 0.55;
};

struct MortgageDelinquency {
  double lateP = 0.04;
  std::int32_t lateDaysMin = 1;
  std::int32_t lateDaysMax = 7;

  double missP = 0.005;
  double partialP = 0.010;
  double cureP = 0.30;
  double clusterMult = 1.6;
  double partialMinFrac = 0.30;
  double partialMaxFrac = 0.80;
  std::int32_t maxCureCycles = 6;
};

struct MortgageTerms {
  MortgageAdoption adoption{};
  MortgagePayment payment{};
  MortgageDelinquency delinquency{};
};

[[nodiscard]] bool
emitMortgage(::PhantomLedger::random::Rng &rng,
             ::PhantomLedger::entity::product::PortfolioRegistry &portfolios,
             ::PhantomLedger::entity::PersonId person, personaTax::Type persona,
             ::PhantomLedger::time::Window window,
             const MortgageTerms &terms = {});

} // namespace PhantomLedger::entities::synth::products
