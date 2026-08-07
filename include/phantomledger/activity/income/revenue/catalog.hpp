#pragma once

#include "phantomledger/activity/income/revenue/profiles.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <array>
#include <optional>

namespace PhantomLedger::activity::income::revenue {

namespace detail {

namespace enumTax = ::PhantomLedger::taxonomies::enums;

// Cash-takings calibration (cash-split-2026-07): the persona cash
// split is research-anchored — sources, derivations and confidence
// tags live in docs/fraud_model_audit.md L-10. Summary: smallBusiness
// .40 = the cash-intensive establishment tier (IRS Cash Intensive
// Businesses ATG sector list; Census establishment mix ~25-30% core);
// freelancer .25 = offline informal work paid in cash (Fed SHED/EIWA);
// salaried .03 = tipped workers ~2.5% of US employment (Yale Budget
// Lab 2024); student .16 = employed students in tipped food-service
// jobs [Derived: employment .40 x ~.4 tipped-job share — recomputed at
// the household-econ-2026-07 student-employment ADJUST].
// Retiree/HNW carry no cash profile BY CHOICE (net cash spenders).

[[nodiscard]] inline constexpr PersonaRevenue freelancerProfile() {
  return {.client = {.activeP = 0.88,
                     .minCount = 2,
                     .maxCount = 5,
                     .paymentsMin = 1,
                     .paymentsMax = 4,
                     .median = 1400.0,
                     .sigma = 0.70},
          .platform = {.activeP = 0.42,
                       .minCount = 1,
                       .maxCount = 2,
                       .paymentsMin = 1,
                       .paymentsMax = 4,
                       .median = 425.0,
                       .sigma = 0.60},
          .ownerDraw = {.activeP = 0.70,
                        .paymentsMin = 1,
                        .paymentsMax = 2,
                        .median = 1800.0,
                        .sigma = 0.75},
          // Trades / markets / offline gigs paid in cash (SHED/EIWA);
          // the lognormal tail stays far below the $10,000 CTR
          // threshold.
          .cashTakings = {.activeP = 0.25,
                          .paymentsMin = 1,
                          .paymentsMax = 4,
                          .median = 450.0,
                          .sigma = 0.60},
          .quietMonth = {.probability = 0.12}};
}

[[nodiscard]] inline constexpr PersonaRevenue smallBusinessProfile() {
  return {.client = {.activeP = 0.55,
                     .minCount = 2,
                     .maxCount = 6,
                     .paymentsMin = 0,
                     .paymentsMax = 3,
                     .median = 2600.0,
                     .sigma = 0.75},
          .platform = {.activeP = 0.22,
                       .minCount = 1,
                       .maxCount = 2,
                       .paymentsMin = 0,
                       .paymentsMax = 3,
                       .median = 950.0,
                       .sigma = 0.70},
          .settlement = {.activeP = 0.74,
                         .paymentsMin = 4,
                         .paymentsMax = 12,
                         .median = 680.0,
                         .sigma = 0.55},
          .ownerDraw = {.activeP = 0.86,
                        .paymentsMin = 1,
                        .paymentsMax = 2,
                        .median = 3400.0,
                        .sigma = 0.70},
          // The cash-intensive tier (restaurants, stores, salons,
          // laundromats, fuel — IRS ATG list) depositing takings at
          // the branch. P(deposit > $10,000) ~ 3.9% at LN(2800, .72)
          // — the legitimate source of CTR filings, calibrated to the
          // FinCEN FY2024 per-adult anchor (L-10 block).
          .cashTakings = {.activeP = 0.40,
                          .paymentsMin = 4,
                          .paymentsMax = 10,
                          .median = 2800.0,
                          .sigma = 0.72},
          .quietMonth = {.probability = 0.06}};
}

[[nodiscard]] inline constexpr PersonaRevenue highNetWorthProfile() {
  return {.ownerDraw = {.activeP = 0.55,
                        .paymentsMin = 1,
                        .paymentsMax = 2,
                        .median = 6000.0,
                        .sigma = 0.65},
          .investment = {.activeP = 0.72,
                         .paymentsMin = 1,
                         .paymentsMax = 3,
                         .median = 12000.0,
                         .sigma = 1.00},
          .quietMonth = {.probability = 0.02}};
}

[[nodiscard]] inline constexpr PersonaRevenue retireeProfile() {
  return {.ownerDraw = {.activeP = 0.33,
                        .paymentsMin = 1,
                        .paymentsMax = 1,
                        .median = 1100.0,
                        .sigma = 0.50},
          .investment = {.activeP = 0.50,
                         .paymentsMin = 1,
                         .paymentsMax = 2,
                         .median = 400.0,
                         .sigma = 0.65},
          .quietMonth = {.probability = 0.05}};
}

// Tipped workers (~2.5% of US employment, Yale Budget Lab 2024)
// depositing cash tips — modest amounts, card tipping now dominates.
[[nodiscard]] inline constexpr PersonaRevenue salariedProfile() {
  return {.cashTakings = {.activeP = 0.03,
                          .paymentsMin = 2,
                          .paymentsMax = 4,
                          .median = 200.0,
                          .sigma = 0.55}};
}

// Employed students in tipped food-service jobs [Derived: student
// employment .40 (L-4, household-econ-2026-07) x ~.4 tipped-job share
// = .16 — the L-10 knock-on recompute the cash-split round queued].
[[nodiscard]] inline constexpr PersonaRevenue studentProfile() {
  return {.cashTakings = {.activeP = 0.16,
                          .paymentsMin = 1,
                          .paymentsMax = 3,
                          .median = 140.0,
                          .sigma = 0.55}};
}

[[nodiscard]] inline constexpr auto buildCatalog() {
  std::array<std::optional<PersonaRevenue>, personas::kKindCount> table{};

  table[enumTax::toIndex(personas::Type::retiree)] = retireeProfile();
  table[enumTax::toIndex(personas::Type::freelancer)] = freelancerProfile();
  table[enumTax::toIndex(personas::Type::smallBusiness)] =
      smallBusinessProfile();
  table[enumTax::toIndex(personas::Type::highNetWorth)] = highNetWorthProfile();
  table[enumTax::toIndex(personas::Type::salaried)] = salariedProfile();
  table[enumTax::toIndex(personas::Type::student)] = studentProfile();

  return table;
}

inline constexpr auto kCatalog = buildCatalog();

} // namespace detail

[[nodiscard]] inline constexpr const std::optional<PersonaRevenue> &
lookupProfile(personas::Type type) noexcept {
  return detail::kCatalog[detail::enumTax::toIndex(type)];
}

} // namespace PhantomLedger::activity::income::revenue
