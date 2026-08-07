#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/utils/groups.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
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

/* Per-person HELD cards. DECLARED CHOICE, and it bounds a distribution with
 * an open top bucket: the Fed SDCPC 2024 card-count law's highest row is
 * "6 or more", which the mean derivation imputes at 7-8, so 8 covers the
 * cited support. This is the HOLDINGS cap; the spend-active subset wired into
 * the hot loop is bounded separately and lower by
 * `actors::kMaxCreditInstruments`. */
inline constexpr std::uint32_t kMaxCardsPerPerson = 8;

using CardsByPerson = primitives::utils::Csr<std::uint32_t, std::uint32_t>;

/* Build an empty per-person card CSR with `kMaxCardsPerPerson` of capacity on
 * every row and zero live entries. Rows are addressed by PersonId-1. */
[[nodiscard]] inline CardsByPerson makeCardsByPerson(std::uint32_t people) {
  const auto rows = static_cast<std::size_t>(people);
  std::vector<std::uint32_t> offsets(rows + 1, 0);
  for (std::size_t i = 0; i <= rows; ++i) {
    offsets[i] = static_cast<std::uint32_t>(i * kMaxCardsPerPerson);
  }
  return CardsByPerson(std::move(offsets),
                       std::vector<std::uint32_t>(rows * kMaxCardsPerPerson, 0),
                       std::vector<std::uint32_t>(rows, 0));
}

struct Registry {
  static constexpr std::uint32_t none =
      std::numeric_limits<std::uint32_t>::max();

  std::vector<Terms> records;
  std::unordered_map<Key, std::uint32_t> byKey;

  /* MULTI-VALUED, and the row order is ISSUANCE ORDER. Slot 0 is the card the
   * primary issuance pass minted; every later slot came from the append pass.
   * That ordering is a contract, not an incident — it is what keeps
   * `forPerson` (which every legacy caller uses) returning the same card it
   * returned before this became a row. */
  CardsByPerson byPerson;

  [[nodiscard]] std::size_t size() const noexcept { return records.size(); }

  [[nodiscard]] bool inRange(PersonId p) const noexcept {
    return p > 0 && static_cast<std::size_t>(p) <= byPerson.rowCount();
  }

  /* Every card index this person holds, in issuance order. */
  [[nodiscard]] std::span<const std::uint32_t>
  indicesFor(PersonId p) const noexcept {
    if (!inRange(p)) {
      return {};
    }
    return byPerson.rowOf(p - 1);
  }

  [[nodiscard]] std::size_t countFor(PersonId p) const noexcept {
    return indicesFor(p).size();
  }

  [[nodiscard]] bool has(PersonId p) const noexcept {
    return !indicesFor(p).empty();
  }

  /* THE FIRST card, not "the" card. Retained because ten call sites in the
   * clearing and export layers genuinely want one representative card and
   * moving them is a separate concern; anything that reasons about a person's
   * card COUNT must use `indicesFor` instead. */
  [[nodiscard]] const Terms *forPerson(PersonId p) const noexcept {
    const auto row = indicesFor(p);
    if (row.empty()) {
      return nullptr;
    }
    return &records[row.front()];
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
