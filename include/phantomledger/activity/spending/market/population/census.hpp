#pragma once

#include "phantomledger/entities/behaviors.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::spending::market::population {

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
};

} // namespace PhantomLedger::spending::market::population
