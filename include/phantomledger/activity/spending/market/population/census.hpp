#pragma once

#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/entities/parties/behaviors.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::activity::spending::market::population {

/// Sparse set of payday day-indices for one person within the run window.
struct PaydaySet {
  std::span<const std::uint32_t> days;

  [[nodiscard]] bool contains(std::uint32_t dayIndex) const noexcept;
};

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
};

} // namespace PhantomLedger::activity::spending::market::population
