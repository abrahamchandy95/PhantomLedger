#pragma once

#include "phantomledger/taxonomies/personas/types.hpp"

#include <array>

namespace PhantomLedger::personas {

// --- Archetypes --------------------------------------------------

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

// Named switch rather than a positional array: the value-to-persona
// mapping is checked by the compiler via the case labels, so future
// enum reorderings cannot silently shift the table.
[[nodiscard]] constexpr Archetype archetype(Type t) noexcept {
  switch (t) {
  case Type::student:
    return {0.7, 0.7, Timing::consumer, 200.0, 0.25, 0.55, 800.0, 0.18, 0.67};

  case Type::retiree:
    return {0.6,  0.9, Timing::consumerDay, 1500.0, 0.55, 0.55, 2500.0,
            0.30, 0.50};

  case Type::freelancer:
    return {1.1, 1.1, Timing::consumer, 900.0, 0.65, 0.65, 4000.0, 0.95, 0.33};

  case Type::smallBusiness:
    return {2.4, 1.8, Timing::business, 8000.0, 0.80, 0.75, 7000.0, 1.50, 0.29};

  case Type::highNetWorth:
    return {1.3,  2.8, Timing::consumer, 25000.0, 0.92, 0.80, 15000.0,
            2.20, 0.11};

  case Type::salaried:
    return {1.0, 1.0, Timing::consumer, 1200.0, 0.70, 0.70, 3000.0, 1.00, 0.40};
  }

  return {};
}

[[nodiscard]] constexpr BetaParams paycheckBeta(Type t) noexcept {
  switch (t) {
  case Type::student:
    return {4.0, 2.0};

  case Type::retiree:
    return {3.0, 3.0};

  case Type::freelancer:
    return {2.0, 4.0};

  case Type::smallBusiness:
    return {2.0, 5.0};

  case Type::highNetWorth:
    return {1.0, 8.0};

  case Type::salaried:
    return {2.0, 3.0};
  }

  return {2.0, 3.0};
}

// Default population share per persona. Computed at compile time so
// any imbalance is caught before runtime.
[[nodiscard]] consteval std::array<double, kKindCount> buildShares() {
  std::array<double, kKindCount> out{};

  out[slot(Type::student)] = 0.12;
  out[slot(Type::retiree)] = 0.10;
  out[slot(Type::freelancer)] = 0.10;
  out[slot(Type::smallBusiness)] = 0.06;
  out[slot(Type::highNetWorth)] = 0.02;

  double assigned = 0.0;
  for (std::size_t i = 0; i < kKindCount; ++i) {
    if (i != slot(Type::salaried)) {
      assigned += out[i];
    }
  }

  if (assigned > 1.0) {
    throw "default persona shares exceed 1.0";
  }

  out[slot(Type::salaried)] = 1.0 - assigned;
  return out;
}

inline constexpr auto kShares = buildShares();

[[nodiscard]] constexpr double share(Type t) noexcept {
  return kShares[slot(t)];
}

} // namespace PhantomLedger::personas
