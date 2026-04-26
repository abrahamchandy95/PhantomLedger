#pragma once

#include "phantomledger/entities/identifiers.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::entity::person {

enum class Flag : std::uint8_t {
  fraud = 1U << 0U,
  mule = 1U << 1U,
  victim = 1U << 2U,
  soloFraud = 1U << 3U,
};

[[nodiscard]] constexpr std::uint8_t bit(Flag f) noexcept {
  return static_cast<std::uint8_t>(f);
}

/// Per-ring slice (offset + length) into one of the topology stores.
struct Slice {
  std::uint32_t offset = 0;
  std::uint32_t size = 0;
};

struct Ring {
  std::uint32_t id = 0;
  Slice members;
  Slice frauds;
  Slice mules;
  Slice victims;
};

struct Roster {
  std::uint32_t count = 0;
  std::vector<std::uint8_t> flags;

  [[nodiscard]] bool has(PersonId p, Flag f) const noexcept {
    return p > 0 && p <= count && (flags[p - 1] & bit(f)) != 0;
  }
};

/// Stores are concatenated across rings; Ring's Slices pick out the
/// contiguous run for a given ring.
struct Topology {
  std::vector<PersonId> memberStore;
  std::vector<PersonId> fraudStore;
  std::vector<PersonId> muleStore;
  std::vector<PersonId> victimStore;
  std::vector<Ring> rings;
};

} // namespace PhantomLedger::entity::person
