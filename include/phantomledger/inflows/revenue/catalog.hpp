#pragma once
/*
 * Only freelancers, small-business owners, and HNW personas have
 * non-payroll revenue streams. The profiles here are baked at
 * compile time and indexed by personas::Kind.
 */

#include "phantomledger/inflows/revenue/profiles.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <array>
#include <optional>

namespace PhantomLedger::inflows::revenue {

namespace detail {

[[nodiscard]] inline constexpr RevenuePersonaProfile freelancerProfile() {
  return RevenuePersonaProfile{
      .client =
          {
              .activeP = 0.88,
              .counterpartiesMin = 2,
              .counterpartiesMax = 5,
              .paymentsMin = 1,
              .paymentsMax = 4,
              .median = 1400.0,
              .sigma = 0.70,
          },
      .platform =
          {
              .activeP = 0.42,
              .counterpartiesMin = 1,
              .counterpartiesMax = 2,
              .paymentsMin = 1,
              .paymentsMax = 4,
              .median = 425.0,
              .sigma = 0.60,
          },
      .settlement = {},
      .ownerDraw =
          {
              .activeP = 0.70,
              .paymentsMin = 1,
              .paymentsMax = 2,
              .median = 1800.0,
              .sigma = 0.75,
          },
      .investment = {},
      .quietMonth = {.probability = 0.12},
  };
}

[[nodiscard]] inline constexpr RevenuePersonaProfile smallBusinessProfile() {
  return RevenuePersonaProfile{
      .client =
          {
              .activeP = 0.55,
              .counterpartiesMin = 2,
              .counterpartiesMax = 6,
              .paymentsMin = 0,
              .paymentsMax = 3,
              .median = 2600.0,
              .sigma = 0.75,
          },
      .platform =
          {
              .activeP = 0.22,
              .counterpartiesMin = 1,
              .counterpartiesMax = 2,
              .paymentsMin = 0,
              .paymentsMax = 3,
              .median = 950.0,
              .sigma = 0.70,
          },
      .settlement =
          {
              .activeP = 0.74,
              .paymentsMin = 4,
              .paymentsMax = 12,
              .median = 680.0,
              .sigma = 0.55,
          },
      .ownerDraw =
          {
              .activeP = 0.86,
              .paymentsMin = 1,
              .paymentsMax = 2,
              .median = 3400.0,
              .sigma = 0.70,
          },
      .investment = {},
      .quietMonth = {.probability = 0.06},
  };
}

[[nodiscard]] inline constexpr RevenuePersonaProfile highNetWorthProfile() {
  return RevenuePersonaProfile{
      .client = {},
      .platform = {},
      .settlement = {},
      .ownerDraw = {},
      .investment =
          {
              .activeP = 0.72,
              .paymentsMin = 0,
              .paymentsMax = 2,
              .median = 6500.0,
              .sigma = 1.05,
          },
      .quietMonth = {.probability = 0.02},
  };
}

/// Build the per-Kind profile table at compile time.
/// Only three kinds have revenue profiles; the rest are nullopt.
[[nodiscard]] inline constexpr auto buildCatalog() {
  std::array<std::optional<RevenuePersonaProfile>, personas::kKindCount>
      table{};

  table[personas::indexOf(personas::Type::freelancer)] = freelancerProfile();

  table[personas::indexOf(personas::Type::smallBusiness)] =
      smallBusinessProfile();

  table[personas::indexOf(personas::Type::highNetWorth)] =
      highNetWorthProfile();

  return table;
}

inline constexpr auto kCatalog = buildCatalog();

} // namespace detail

/// Look up the revenue profile for a persona kind.
/// Returns nullopt for kinds that have no non-payroll revenue
/// (student, retiree, salaried).
[[nodiscard]] inline constexpr const std::optional<RevenuePersonaProfile> &
archetypeFor(personas::Type type) noexcept {
  return detail::kCatalog[personas::indexOf(type)];
}

} // namespace PhantomLedger::inflows::revenue
