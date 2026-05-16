#pragma once
#include "phantomledger/entities/identifiers.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace PhantomLedger::entity::account {

enum class Flag : std::uint8_t {
  fraud = 1U << 0U,
  mule = 1U << 1U,
  victim = 1U << 2U,
  external = 1U << 3U,
  shell = 1U << 4U,
};

[[nodiscard]] constexpr std::uint8_t bit(Flag f) noexcept {
  return static_cast<std::uint8_t>(f);
}

[[nodiscard]] constexpr bool hasFlag(std::uint8_t flags, Flag f) noexcept {
  return (flags & bit(f)) != 0;
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

struct Lookup {
  std::unordered_map<Key, std::uint32_t> byId;
};

struct Ownership {
  std::vector<std::uint32_t> byPersonOffset;
  std::vector<std::uint32_t> byPersonIndex;

  [[nodiscard]] std::uint32_t primaryIndex(PersonId person) const noexcept {
    return byPersonIndex[byPersonOffset[person - 1]];
  }
};

} // namespace PhantomLedger::entity::account
