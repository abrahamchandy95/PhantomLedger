#pragma once

#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <array>
#include <cstddef>

namespace PhantomLedger::personas {

namespace enumTax = ::PhantomLedger::taxonomies::enums;

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

namespace detail {

static_assert(enumTax::isIndexable(kTypes));

inline constexpr auto kArchetypes = std::to_array<Archetype>({
    {0.7, 0.7, Timing::consumer, 200.0, 0.25, 0.55, 800.0, 0.18, 0.67},
    {0.6, 0.9, Timing::consumerDay, 1500.0, 0.55, 0.55, 2500.0, 0.30, 0.50},
    {1.1, 1.1, Timing::consumer, 900.0, 0.65, 0.65, 4000.0, 0.95, 0.33},
    {2.4, 1.8, Timing::business, 8000.0, 0.80, 0.75, 7000.0, 1.50, 0.29},
    {1.3, 2.8, Timing::consumer, 25000.0, 0.92, 0.80, 15000.0, 2.20, 0.11},
    {1.0, 1.0, Timing::consumer, 1200.0, 0.70, 0.70, 3000.0, 1.00, 0.40},
});

inline constexpr auto kPaycheckBetas = std::to_array<BetaParams>({
    {4.0, 2.0}, // student
    {3.0, 3.0}, // retiree
    {2.0, 4.0}, // freelancer
    {2.0, 5.0}, // smallBusiness
    {1.0, 8.0}, // highNetWorth
    {2.0, 3.0}, // salaried
});

static_assert(kArchetypes.size() == kTypeCount);
static_assert(kPaycheckBetas.size() == kTypeCount);

} // namespace detail

[[nodiscard]] constexpr Archetype archetype(Type type) noexcept {
  return detail::kArchetypes[enumTax::toIndex(type)];
}

[[nodiscard]] constexpr BetaParams paycheckBeta(Type type) noexcept {
  return detail::kPaycheckBetas[enumTax::toIndex(type)];
}

// Default population share per persona.
[[nodiscard]] consteval std::array<double, kTypeCount> buildShares() {
  std::array<double, kTypeCount> out{};

  out[enumTax::toIndex(Type::student)] = 0.12;
  out[enumTax::toIndex(Type::retiree)] = 0.10;
  out[enumTax::toIndex(Type::freelancer)] = 0.10;
  out[enumTax::toIndex(Type::smallBusiness)] = 0.06;
  out[enumTax::toIndex(Type::highNetWorth)] = 0.02;

  double assigned = 0.0;
  for (std::size_t index = 0; index < kTypeCount; ++index) {
    if (index != enumTax::toIndex(Type::salaried)) {
      assigned += out[index];
    }
  }

  if (assigned > 1.0) {
    throw "default persona shares exceed 1.0";
  }

  out[enumTax::toIndex(Type::salaried)] = 1.0 - assigned;
  return out;
}

inline constexpr auto kShares = buildShares();

[[nodiscard]] constexpr double share(Type type) noexcept {
  return kShares[enumTax::toIndex(type)];
}

} // namespace PhantomLedger::personas
