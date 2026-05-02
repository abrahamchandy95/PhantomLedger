#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/products/portfolio.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/taxonomies/personas/table.hpp"

#include <cstdint>

namespace PhantomLedger::entities::synth::products {

namespace personaTax = ::PhantomLedger::personas;

struct StudentLoanAdoption {
  personaTax::Rates ownership{
      .student = 0.85,
      .retiree = 0.05,
      .freelancer = 0.20,
      .smallBusiness = 0.20,
      .highNetWorth = 0.10,
      .salaried = 0.30,
  };

  [[nodiscard]] constexpr double
  probability(personaTax::Type type) const noexcept {
    return ownership.get(type);
  }
};

struct StudentLoanPlanMix {
  double standardP = 0.65;
  double extendedP = 0.20;
  double idrLikeP = 0.15;
};

struct StudentLoanTerm {
  std::int32_t standardMonths = 120;
  std::int32_t extendedMonths = 240;

  std::int32_t idr20YearMonths = 240;
  std::int32_t idr25YearMonths = 300;
  double idr20YearP = 0.55;
};

struct StudentLoanDeferment {
  std::int32_t gracePeriodMonths = 6;
  double studentP = 0.65;
};

struct StudentLoanPayment {
  double median = 295.0;
  double sigma = 0.55;
  double floor = 50.0;
};

struct StudentLoanDelinquency {
  double lateP = 0.06;
  std::int32_t lateDaysMin = 1;
  std::int32_t lateDaysMax = 14;

  double missP = 0.015;
  double partialP = 0.020;
  double cureP = 0.25;

  double clusterMult = 1.8;
  double partialMinFrac = 0.30;
  double partialMaxFrac = 0.80;

  std::int32_t maxCureCycles = 4;
};

struct StudentLoanTerms {
  StudentLoanAdoption adoption{};
  StudentLoanPlanMix planMix{};
  StudentLoanTerm term{};
  StudentLoanDeferment deferment{};
  StudentLoanPayment payment{};
  StudentLoanDelinquency delinquency{};
};

[[nodiscard]] bool
emitStudentLoan(::PhantomLedger::random::Rng &rng,
                ::PhantomLedger::entity::product::PortfolioRegistry &portfolios,
                ::PhantomLedger::entity::PersonId person,
                personaTax::Type persona, ::PhantomLedger::time::Window window,
                const StudentLoanTerms &terms = {});

} // namespace PhantomLedger::entities::synth::products
