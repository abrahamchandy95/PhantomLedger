#pragma once

#include "phantomledger/entities/identifiers.hpp"

#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

namespace PhantomLedger::entity::card {

// Autopay configuration drawn at issuance.
enum class Autopay : std::uint8_t {
  manual = 0,
  minimum = 1,
  full = 2,
};

// Per-card fixed terms drawn at issuance time
struct Terms {
  Key key{};
  PersonId owner = invalidPerson;
  /// Annualized interest rate
  double apr = 0.0;
  double creditLimit = 0.0;
  std::uint8_t cycleDay = 1;
  Autopay autopay = Autopay::manual;
};

struct Registry {
  static constexpr std::uint32_t none =
      std::numeric_limits<std::uint32_t>::max();

  std::vector<Terms> records;
  std::unordered_map<Key, std::uint32_t> byKey;
  std::vector<std::uint32_t> byPerson;

  [[nodiscard]] std::size_t size() const noexcept { return records.size(); }

  [[nodiscard]] bool has(PersonId p) const noexcept {
    return p > 0 && static_cast<std::size_t>(p) <= byPerson.size() &&
           byPerson[p - 1] != none;
  }

  [[nodiscard]] const Terms *forPerson(PersonId p) const noexcept {
    if (!has(p)) {
      return nullptr;
    }
    return &records[byPerson[p - 1]];
  }

  [[nodiscard]] const Terms *forKey(const Key &k) const noexcept {
    const auto it = byKey.find(k);
    if (it == byKey.end()) {
      return nullptr;
    }
    return &records[it->second];
  }
};

} // namespace PhantomLedger::entity::card
