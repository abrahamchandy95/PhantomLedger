#pragma once

#include "phantomledger/entities/identifiers.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace PhantomLedger::relationships::family {

struct Graph {
  // -------------------------------------------------------------------------
  // Households (CSR)
  // -------------------------------------------------------------------------

  std::vector<entity::PersonId> householdMembers;

  std::vector<std::uint32_t> householdOffsets;

  std::vector<std::uint32_t> householdOf;

  // -------------------------------------------------------------------------
  // Immediate family
  // -------------------------------------------------------------------------

  std::vector<entity::PersonId> spouseOf;

  std::vector<std::array<entity::PersonId, 2>> parentsOf;

  std::vector<std::vector<entity::PersonId>> childrenOf;

  // -------------------------------------------------------------------------
  // Cross-household financial-support ties
  // -------------------------------------------------------------------------

  std::vector<std::vector<entity::PersonId>> supportedParentsBy;

  std::vector<std::vector<entity::PersonId>> supportingChildrenOf;

  // -------------------------------------------------------------------------
  // Constants
  // -------------------------------------------------------------------------

  static constexpr std::uint32_t kNoHousehold =
      std::numeric_limits<std::uint32_t>::max();

  // -------------------------------------------------------------------------
  // Accessors (zero-allocation reads)
  // -------------------------------------------------------------------------

  [[nodiscard]] std::uint32_t personCount() const noexcept {
    return static_cast<std::uint32_t>(householdOf.size());
  }

  [[nodiscard]] std::uint32_t householdCount() const noexcept {
    return householdOffsets.empty()
               ? 0U
               : static_cast<std::uint32_t>(householdOffsets.size() - 1);
  }

  [[nodiscard]] std::span<const entity::PersonId>
  householdMembersOf(std::uint32_t householdIdx) const noexcept {
    const auto lo = householdOffsets[householdIdx];
    const auto hi = householdOffsets[householdIdx + 1];
    return {householdMembers.data() + lo, hi - lo};
  }

  [[nodiscard]] entity::PersonId spouseFor(entity::PersonId p) const noexcept {
    if (!entity::valid(p) || p > spouseOf.size()) {
      return entity::invalidPerson;
    }
    return spouseOf[p - 1];
  }

  [[nodiscard]] std::array<entity::PersonId, 2>
  parentsFor(entity::PersonId p) const noexcept {
    if (!entity::valid(p) || p > parentsOf.size()) {
      return {entity::invalidPerson, entity::invalidPerson};
    }
    return parentsOf[p - 1];
  }
};

} // namespace PhantomLedger::relationships::family
