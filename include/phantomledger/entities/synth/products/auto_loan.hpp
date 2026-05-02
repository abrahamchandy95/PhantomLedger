#pragma once

#include "phantomledger/taxonomies/personas/table.hpp"

#include <cstdint>

namespace PhantomLedger::entities::synth::products {

namespace personaTax = ::PhantomLedger::personas;

struct AutoLoanAdoption {
  personaTax::Rates ownership{
      .student = 0.10,
      .retiree = 0.20,
      .freelancer = 0.40,
      .smallBusiness = 0.45,
      .highNetWorth = 0.45,
      .salaried = 0.45,
  };

  [[nodiscard]] constexpr double
  probability(personaTax::Type type) const noexcept {
    return ownership.get(type);
  }
};

struct AutoLoanVehicleMix {
  double newVehicleShare = 0.35;
};

struct AutoLoanPayment {
  double newMedian = 715.0;
  double newSigma = 0.30;

  double usedMedian = 525.0;
  double usedSigma = 0.35;

  double floor = 100.0;
};

struct AutoLoanTerm {
  double newMeanMonths = 68.0;
  double newSigmaMonths = 6.0;

  double usedMeanMonths = 67.0;
  double usedSigmaMonths = 8.0;

  std::int32_t minMonths = 24;
  std::int32_t maxMonths = 84;
};

struct AutoLoanDelinquency {
  double lateP = 0.05;
  std::int32_t lateDaysMin = 1;
  std::int32_t lateDaysMax = 10;

  double missP = 0.010;
  double partialP = 0.015;
  double cureP = 0.30;

  double clusterMult = 1.7;
  double partialMinFrac = 0.30;
  double partialMaxFrac = 0.80;

  std::int32_t maxCureCycles = 4;
};

struct AutoLoanTerms {
  AutoLoanAdoption adoption{};
  AutoLoanVehicleMix vehicleMix{};
  AutoLoanPayment payment{};
  AutoLoanTerm term{};
  AutoLoanDelinquency delinquency{};
};

} // namespace PhantomLedger::entities::synth::products
