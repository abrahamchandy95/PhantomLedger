#pragma once

#include "phantomledger/activity/spending/market/bounds.hpp"
#include "phantomledger/activity/spending/market/cards.hpp"
#include "phantomledger/activity/spending/market/commerce/network.hpp"
#include "phantomledger/activity/spending/market/market.hpp"
#include "phantomledger/activity/spending/market/population/census.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/taxonomies/merchants/types.hpp"

#include "phantomledger/primitives/random/rng.hpp"
#include <array>

#include <algorithm>
#include <cstdint>

namespace PhantomLedger::activity::spending::market {

inline constexpr std::array<merchants::Category, 4> kBillerCategories{
    merchants::Category::utilities,
    merchants::Category::telecom,
    merchants::Category::insurance,
    merchants::Category::education,
};

[[nodiscard]] constexpr bool isBillerCategory(merchants::Category c) noexcept {
  for (const auto cat : kBillerCategories) {
    if (cat == c) {
      return true;
    }
  }
  return false;
}

struct MerchantPickBudget {
  std::uint16_t maxPickAttempts = 250;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] {
      v::ge("maxPickAttempts", static_cast<int>(maxPickAttempts), 1);
    });
  }
};

struct ExplorationDistribution {
  double alpha = 1.6;
  double beta = 9.5;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::gt("alpha", alpha, 0.0); });
    r.check([&] { v::gt("beta", beta, 0.0); });
  }
};

struct BurstSchedule {
  /* BURSTS ARE A PER-YEAR RATE, never a per-run probability. `probability =
   * 0.08` applied ONCE PER RUN over `[0, days)` means 0.49/year at the 60-day
   * golden config and 0.004/year over a 20-year window — measured, a
   * two-decade run gave 92% of people no spending burst at all and the rest
   * exactly one.
   *
   * THE LEVEL IS DERIVED, NOT RE-INVENTED: 0.08 per 60 days is
   * 0.08 * 365.25/60 = 0.487/year, so this reproduces the 60-day config every
   * existing band was calibrated against and scopes the fix to the
   * window-dependence alone. A 20-year run yields ~9.7 windows per person.
   *
   * CLASS S, level DERIVED-FROM-PRIOR-BEHAVIOUR. ~1 notable spending episode
   * every two years is defensible for a vacation/holiday shape, but the
   * anchor is continuity with the calibrated configs, not a source. */
  double burstsPerYear = 0.487;
  std::uint16_t minDays = 3;
  std::uint16_t maxDays = 9;

  /* Hard ceiling on stored windows per person, so a pathological window
   * length cannot blow up the schedule. At 0.487/year this is ~60 years of
   * headroom; exceeding it is logged by the gate, never silent. */
  std::uint16_t maxWindowsPerPerson = 32;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::nonNegative("burstsPerYear", burstsPerYear); });
    r.check([&] { v::ge("minDays", static_cast<int>(minDays), 1); });
    r.check([&] {
      v::ge("maxDays", static_cast<int>(maxDays), static_cast<int>(minDays));
    });
  }
};

struct MarketSources {
  Bounds bounds{};
  population::Census census{};
  commerce::Network network{};
  Cards cards{};
  std::uint64_t baseSeed = 0;
};

struct PayeeSelectionRules {
  MerchantPickBudget picking{};

  // Counts plumbed from the world Merchants rules.
  std::uint16_t favoriteMin = 8;
  std::uint16_t favoriteMax = 30;

  /* Slots reserved per person so the monthly evolution pass can add in
   * place. MUST BE >= favoriteMax AND >= the evolution config's
   * maxFavorites, or an add silently no-ops. */
  std::uint16_t favoriteCapacity = 48;
  std::uint16_t billerMin = 2;
  std::uint16_t billerMax = 6;

  /* Slots reserved per person for biller churn. A closed utility is REPLACED
   * rather than merely dropped (the obligation persists), so the count is
   * stable and the headroom only has to absorb transient states. */
  std::uint16_t billerCapacity = 10;
};

/* ONE PERSON'S BURST WINDOWS for a window of `days`.
 *
 * KEEP THIS EXTRACTED so the gate MEASURES the construction rather than
 * re-deriving it. A gate that reimplements the rule it checks passes
 * vacuously the moment production changes and the copy does not.
 *
 * A RATE IS NOT A PROBABILITY UNTIL MULTIPLIED BY A DURATION: each 365-day
 * segment draws at `burstsPerYear * segmentSpan / 365.25`, so a 60-day
 * window reproduces the 0.08 per-run coin exactly while a 20-year window
 * yields ~9.7 windows. Applying the bare annual rate to a partial year
 * multiplies prevalence ~6x at every calibrated config. */
[[nodiscard]] inline std::vector<commerce::BurstWindow>
buildPersonBursts(::PhantomLedger::random::Rng &rng, const BurstSchedule &rules,
                  int days) {
  std::vector<commerce::BurstWindow> out;
  if (days <= 0) {
    return out;
  }
  for (int segmentStart = 0; segmentStart < days; segmentStart += 365) {
    if (out.size() >= rules.maxWindowsPerPerson) {
      break;
    }
    const int segmentSpan = std::min(365, days - segmentStart);
    const double p = std::min(
        1.0, rules.burstsPerYear * static_cast<double>(segmentSpan) / 365.25);
    if (!rng.coin(p)) {
      continue;
    }
    const auto start = static_cast<std::uint32_t>(
        segmentStart + rng.uniformInt(0, segmentSpan));
    const auto len = static_cast<std::uint16_t>(
        rng.uniformInt(rules.minDays, rules.maxDays + 1));
    out.push_back(commerce::BurstWindow{start, len});
  }
  return out;
}

struct ShopperBehaviorRules {
  BurstSchedule burst{};
  ExplorationDistribution exploration{};
};

[[nodiscard]] Market buildMarket(MarketSources sources,
                                 PayeeSelectionRules payees = {},
                                 ShopperBehaviorRules behavior = {});

} // namespace PhantomLedger::activity::spending::market
