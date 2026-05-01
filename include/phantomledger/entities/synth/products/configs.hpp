#pragma once

#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <array>
#include <cstdint>

namespace PhantomLedger::entities::synth::products {

namespace personas = ::PhantomLedger::personas;
namespace enums = ::PhantomLedger::taxonomies::enums;

static_assert(enums::isIndexable(personas::kTypes));

struct PerPersonaP {
  std::array<double, personas::kTypeCount> byType{};

  [[nodiscard]] constexpr double at(personas::Type type) const noexcept {
    return byType[enums::toIndex(type)];
  }
};

struct MortgageConfig {
  PerPersonaP ownership = {{
      /* student        */ 0.02,
      /* retiree        */ 0.55,
      /* freelancer     */ 0.30,
      /* smallBusiness  */ 0.55,
      /* highNetWorth   */ 0.65,
      /* salaried       */ 0.55,
  }};

  double paymentMedian = 1750.0;
  double paymentSigma = 0.55;

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

inline constexpr MortgageConfig kDefaultMortgage{};

struct AutoLoanConfig {
  PerPersonaP ownership = {{
      /* student        */ 0.10,
      /* retiree        */ 0.20,
      /* freelancer     */ 0.40,
      /* smallBusiness  */ 0.45,
      /* highNetWorth   */ 0.45,
      /* salaried       */ 0.45,
  }};

  /// New-vs-used segment split. Conditional on already having a loan.
  double newVehicleShare = 0.35;

  /// Per-segment monthly payment medians + sigmas. New cars finance
  /// at higher payments; used cars cluster lower.
  double newPaymentMedian = 715.0;
  double newPaymentSigma = 0.30;
  double usedPaymentMedian = 525.0;
  double usedPaymentSigma = 0.35;

  /// Per-segment contractual term (months), drawn from a truncated
  /// normal around the segment mean.
  double newTermMeanMonths = 68.0;
  double newTermSigmaMonths = 6.0;
  double usedTermMeanMonths = 67.0;
  double usedTermSigmaMonths = 8.0;
  std::int32_t termMinMonths = 24;
  std::int32_t termMaxMonths = 84;

  // Delinquency knobs.
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

inline constexpr AutoLoanConfig kDefaultAutoLoan{};

struct StudentLoanConfig {
  PerPersonaP ownership = {{
      /* student        */ 0.85,
      /* retiree        */ 0.05,
      /* freelancer     */ 0.20,
      /* smallBusiness  */ 0.20,
      /* highNetWorth   */ 0.10,
      /* salaried       */ 0.30,
  }};

  double standardPlanP = 0.65;
  double extendedPlanP = 0.20;
  double idrLikePlanP = 0.15;

  std::int32_t standardTermMonths = 120;
  std::int32_t extendedTermMonths = 240;
  std::int32_t idr20YearTermMonths = 240;
  std::int32_t idr25YearTermMonths = 300;
  double idr20YearP = 0.55;

  std::int32_t gracePeriodMonths = 6;

  double studentDefermentP = 0.65;

  /// Monthly payment lognormal-by-median + sigma.
  double paymentMedian = 295.0;
  double paymentSigma = 0.55;

  // Delinquency knobs.
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

inline constexpr StudentLoanConfig kDefaultStudentLoan{};

struct TaxConfig {
  PerPersonaP ownership = {{
      /* student        */ 0.05,
      /* retiree        */ 0.20,
      /* freelancer     */ 0.65,
      /* smallBusiness  */ 0.85,
      /* highNetWorth   */ 0.50,
      /* salaried       */ 0.10,
  }};

  double quarterlyAmountMedian = 1250.0;
  double quarterlyAmountSigma = 0.65;

  double refundP = 0.65;
  double balanceDueP = 0.20;

  double refundAmountMedian = 1850.0;
  double refundAmountSigma = 0.55;

  double balanceDueAmountMedian = 1100.0;
  double balanceDueAmountSigma = 0.65;
};

inline constexpr TaxConfig kDefaultTax{};

struct InsuranceTargetsByPersona {
  PerPersonaP autoP = {{
      /* student        */ 0.30,
      /* retiree        */ 0.85,
      /* freelancer     */ 0.85,
      /* smallBusiness  */ 0.90,
      /* highNetWorth   */ 0.95,
      /* salaried       */ 0.92,
  }};
  PerPersonaP homeP = {{
      /* student        */ 0.05,
      /* retiree        */ 0.55,
      /* freelancer     */ 0.30,
      /* smallBusiness  */ 0.55,
      /* highNetWorth   */ 0.70,
      /* salaried       */ 0.55,
  }};
  PerPersonaP lifeP = {{
      /* student        */ 0.10,
      /* retiree        */ 0.55,
      /* freelancer     */ 0.30,
      /* smallBusiness  */ 0.45,
      /* highNetWorth   */ 0.55,
      /* salaried       */ 0.55,
  }};
};

struct InsuranceConfig {
  InsuranceTargetsByPersona targets{};

  double mortgageHomeRequiredP = 0.99;
  double autoLoanAutoRequiredP = 0.997;

  double autoMedian = 225.0;
  double autoSigma = 0.30;
  double homeMedian = 163.0;
  double homeSigma = 0.30;
  double lifeMedian = 28.0;
  double lifeSigma = 0.40;

  double autoClaimAnnualP = 0.042;
  double homeClaimAnnualP = 0.055;
};

inline constexpr InsuranceConfig kDefaultInsurance{};

} // namespace PhantomLedger::entities::synth::products
