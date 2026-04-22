#pragma once
/*
 persona behavioral parameters.
 *
 * Archetypes carry rate/amount multipliers, card probabilities,
 * and paycheck sensitivity used by downstream generators to shape
 * spending behavior.
 */

#include "phantomledger/taxonomies/personas/names.hpp"

#include <array>
#include <string_view>

namespace PhantomLedger::personas {

struct Archetype {
  double rateMultiplier;
  double amountMultiplier;
  Timing timing;
  double initialBalance;
  double cardProb;
  double ccShare;
  double creditLimit;
  double weight;
  double paycheckSensitivity;
};

struct BetaParams {
  double alpha;
  double beta;
};

namespace detail {

inline constexpr std::array<Archetype, kKindCount> kArchetypesTable{{
    // student
    {0.7, 0.7, Timing::consumer, 200.0, 0.25, 0.55, 800.0, 0.18, 0.67},
    // retiree
    {0.6, 0.9, Timing::consumerDay, 1500.0, 0.55, 0.55, 2500.0, 0.30, 0.50},
    // freelancer
    {1.1, 1.1, Timing::consumer, 900.0, 0.65, 0.65, 4000.0, 0.95, 0.33},
    // smallBusiness
    {2.4, 1.8, Timing::business, 8000.0, 0.80, 0.75, 7000.0, 1.50, 0.29},
    // highNetWorth
    {1.3, 2.8, Timing::consumer, 25000.0, 0.92, 0.80, 15000.0, 2.20, 0.11},
    // salaried
    {1.0, 1.0, Timing::consumer, 1200.0, 0.70, 0.70, 3000.0, 1.00, 0.40},
}};

[[nodiscard]] consteval std::array<double, kKindCount> buildDefaultFractions() {
  std::array<double, kKindCount> fractions{};

  fractions[indexOf(Type::student)] = 0.12;
  fractions[indexOf(Type::retiree)] = 0.10;
  fractions[indexOf(Type::freelancer)] = 0.10;
  fractions[indexOf(Type::smallBusiness)] = 0.06;
  fractions[indexOf(Type::highNetWorth)] = 0.02;

  const double assigned = fractions[indexOf(Type::student)] +
                          fractions[indexOf(Type::retiree)] +
                          fractions[indexOf(Type::freelancer)] +
                          fractions[indexOf(Type::smallBusiness)] +
                          fractions[indexOf(Type::highNetWorth)];

  if (assigned > 1.0) {
    throw "persona fractions exceed 1.0";
  }

  fractions[indexOf(Type::salaried)] = 1.0 - assigned;
  return fractions;
}

inline constexpr std::array<BetaParams, kKindCount>
    kPaycheckSensitivityBetasTable{{
        {4.0, 2.0}, // student
        {3.0, 3.0}, // retiree
        {2.0, 4.0}, // freelancer
        {2.0, 5.0}, // smallBusiness
        {1.0, 8.0}, // highNetWorth
        {2.0, 3.0}, // salaried
    }};

} // namespace detail

inline constexpr auto kArchetypes = detail::kArchetypesTable;
inline constexpr auto kDefaultFractions = detail::buildDefaultFractions();
inline constexpr auto kPaycheckSensitivityBetas =
    detail::kPaycheckSensitivityBetasTable;

[[nodiscard]] constexpr const Archetype &archetype(Type type) noexcept {
  return kArchetypes[indexOf(type)];
}

[[nodiscard]] constexpr const Archetype &
archetype(std::string_view s) noexcept {
  return archetype(fromString(s));
}

[[nodiscard]] constexpr double defaultFraction(Type type) noexcept {
  return kDefaultFractions[indexOf(type)];
}

[[nodiscard]] constexpr BetaParams paycheckSensitivityBeta(Type type) noexcept {
  return kPaycheckSensitivityBetas[indexOf(type)];
}

} // namespace PhantomLedger::personas
