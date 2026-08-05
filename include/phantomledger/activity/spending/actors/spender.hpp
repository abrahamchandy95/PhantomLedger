#pragma once

#include "phantomledger/activity/spending/market/commerce/view.hpp"
#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/entities/parties/behaviors.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <cstdint>
#include <span>
#include <limits>

namespace PhantomLedger::activity::spending::actors {

// H2 step 2c (macro-history-v1): the retirement consumption step —
// ticket amounts scale by this level factor from the claiming day
// onward. Aguiar-Hurst (JPE 2005) retirement-consumption drop, ~-12%
// (declared band 10-15%, authority U-7). Applies only to spenders whose
// SEED archetype is a working type: a seed retiree's archetype already
// encodes retired-calibrated spending (rate x0.6 / amount x0.9 in the
// audited persona table), so stacking the step would double-count.
inline constexpr double kRetiredSpendScale = 0.88;

/// Per-person data needed by the spending hot loop.

struct Spender {

  static constexpr std::uint32_t kInvalidLedgerIndex =
      std::numeric_limits<std::uint32_t>::max();

  // "Never retires in this window" (mirrors population::kNoRetirementDay).
  static constexpr std::uint32_t kNoRetireDay =
      std::numeric_limits<std::uint32_t>::max();

  // "Survives this window" (mirrors population::kNoDeathDay).
  static constexpr std::uint32_t kNoDeathDay =
      std::numeric_limits<std::uint32_t>::max();

  // Identity
  entity::PersonId person = entity::invalidPerson;
  std::uint32_t personIndex = 0;

  // Routing destinations (Keys + their resolved Ledger indices).
  entity::Key depositAccount{};
  entity::Key card{};
  std::uint32_t depositAccountIdx = kInvalidLedgerIndex;
  std::uint32_t cardIdx = kInvalidLedgerIndex;
  bool hasCard = false;

  // Persona type + back-pointer for cold fields not cached below.
  personas::Type personaType{};
  const entity::behavior::Persona *persona = nullptr;

  // --- Hot-path persona fields (cached at prepareSpenders time) ---
  double rateMultiplier = 1.0;
  double amountMultiplier = 1.0;
  double cardShare = 0.0;
  personas::Timing timing{};

  // Per-person merchant pool sizes (still derived from CSR row sizes).
  std::uint16_t favCount = 0;
  std::uint16_t billCount = 0;

  float exploreProp = 0.0f;
  // burst-rate-2026-07: every burst window this person has over the run,
  // not just the first. Points into the market's schedule, which outlives
  // the Spender.
  std::span<const market::commerce::BurstWindow> bursts{};

  // relocation-2026-07: `homeArea` IS DELIBERATELY GONE from this struct.
  //
  // It used to cache the home area at `prepareSpenders` time, and
  // `prepareSpenders` runs ONCE for the whole fold — so once homes could move,
  // that cache was a value frozen at window start while the schedule and the
  // exporter moved on. Removing the field rather than leaving it stale is what
  // makes the compiler find every reader; `payments.cpp` now asks
  // `market.population().homeArea(person)`, which the monthly evolver
  // re-points. Do not reintroduce it: a per-day copy would need `runDay` to
  // stop taking `PreparedRun` by const ref, and reading fresh costs one
  // indirection on a path that already does a CDF walk.

  // H2 step 2c: the window day-index from which kRetiredSpendScale
  // applies (kNoRetireDay = never in this window). From the
  // persona-timeline carrier via the population View.
  std::uint32_t retireDay = kNoRetireDay;

  // H3: the window day-index of the person's death — the emission loop
  // stops this spender's person-days there (kNoDeathDay = survives the
  // window). Same carrier provenance as retireDay.
  std::uint32_t deathDay = kNoDeathDay;
};

} // namespace PhantomLedger::activity::spending::actors
