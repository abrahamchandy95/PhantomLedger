#pragma once
#include "phantomledger/entities/identifier/key.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace PhantomLedger::entity::account {

enum class Flag : std::uint8_t {
  fraud = 1U << 0U,
  mule = 1U << 1U,
  victim = 1U << 2U,
  external = 1U << 3U,
};

[[nodiscard]] constexpr std::uint8_t bit(Flag f) noexcept {
  return static_cast<std::uint8_t>(f);
}

struct Record {
  Key id;
  PersonId owner = invalidPerson;
  std::uint8_t flags = 0;
};

struct Registry {
  std::vector<Record> records;

  [[nodiscard]] bool hasOwner(std::uint32_t recIx) const noexcept {
    return records[recIx].owner != invalidPerson;
  }
};

/// Inverted index from account Key to record offset.
struct Lookup {
  std::unordered_map<Key, std::uint32_t> byId;
};

/// Per-person view over the Registry. byPersonOffset is a CSR-style
/// offset array of size `people.count + 1`; byPersonIndex holds the
/// concatenated record indices, in person order.
struct Ownership {
  std::vector<std::uint32_t> byPersonOffset;
  std::vector<std::uint32_t> byPersonIndex;

  [[nodiscard]] std::uint32_t primaryIndex(PersonId person) const noexcept {
    return byPersonIndex[byPersonOffset[person - 1]];
  }
};

} // namespace PhantomLedger::entity::account
