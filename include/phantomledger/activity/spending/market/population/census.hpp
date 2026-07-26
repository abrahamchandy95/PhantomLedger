#pragma once

#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/entities/parties/behaviors.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace PhantomLedger::activity::spending::market::population {

/// Sparse set of payday day-indices for one person within the run window.
struct PaydaySet {
  std::span<const std::uint32_t> days;

  [[nodiscard]] bool contains(std::uint32_t dayIndex) const noexcept;
};

// H2 step 2c (macro-history-v1): "no retirement consumption step in this
// window" sentinel for Census::retirementDays / View::retirementDay.
inline constexpr std::uint32_t kNoRetirementDay =
    std::numeric_limits<std::uint32_t>::max();

// H3 (macro-history-v1): "does not die in this window" sentinel for
// Census::deathDays / View::deathDay.
inline constexpr std::uint32_t kNoDeathDay =
    std::numeric_limits<std::uint32_t>::max();

struct Census {
  std::uint32_t count = 0;

  // Per-person, indexed by PersonId-1.
  std::span<const entity::Key> primaryAccounts;
  std::span<const personas::Type> personaTypes;
  std::span<const entity::behavior::Persona> personaObjects;

  // Payday rosters: one PaydaySet per person.
  std::span<const PaydaySet> paydays;

  // geo-causal-v1 (G2a): per-person home area (PersonId-1), from the
  // compact People::homeAreas carrier. EMPTY on the monolith reference
  // oracle (addSpending has no People in scope); the production windowed
  // path and the test world supply it. UNREAD until G2a step-2 wires
  // distance-decay selection, so an empty span moves no golden.
  std::span<const entity::geography::GeoAreaId> homeAreas;

  // H2 step 2c: the window day-index from which the retirement
  // consumption step applies (kNoRetirementDay = never in this window).
  // Computed by the transfers layer from the persona-timeline carrier
  // (the blueprint's pack), which BOTH engines share — so unlike
  // homeAreas this is never empty on the oracle. Seed retirees and
  // highNetWorth carry the sentinel: a seed retiree's archetype already
  // encodes retired-calibrated spending (rate x0.6 / amount x0.9), so
  // the step models only the IN-WINDOW transition.
  std::span<const std::uint32_t> retirementDays;

  // H3: the window day-index of the person's DEATH (kNoDeathDay = the
  // person survives the window). Same provenance as retirementDays —
  // the blueprint pack's timeline lane carries tl.death, both engines
  // identical. The emission loop stops a spender's person-days here;
  // no exemptions (everyone dies).
  std::span<const std::uint32_t> deathDays;
};

} // namespace PhantomLedger::activity::spending::market::population
