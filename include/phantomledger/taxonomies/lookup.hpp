#pragma once
/*
 * Compile-time name ↔ enum lookup machinery.
 *
 * Three taxonomy modules (channels, merchants, personas) used to each
 * ship their own copy of:
 *
 *   - consteval insertion sort by name
 *   - consteval duplicate-name check
 *   - consteval reverse table (value → name), indexed by some
 *     user-defined slot function
 *   - runtime binary search (name → optional<value>)
 *
 * This header is that code once. Each taxonomy module defines:
 *
 *   - the Value type (an enum)
 *   - a kEntries array of Entry<Value>
 *   - (optionally) an Indexer that maps Value → size_t slot for the
 *     reverse table
 *
 * and gets a parse() and a name() that behave identically to what
 * the hand-written versions did.
 */

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace PhantomLedger::lookup {

template <class Value> struct Entry {
  std::string_view name;
  Value value;
};

// Insertion sort in a consteval context — std::sort isn't constexpr
// until C++26, and the tables here are tiny enough that asymptotic
// complexity is irrelevant.
template <class Value, std::size_t N>
[[nodiscard]] consteval std::array<Entry<Value>, N>
sorted(std::array<Entry<Value>, N> entries) {
  for (std::size_t i = 1; i < N; ++i) {
    auto key = entries[i];
    std::size_t j = i;
    while (j > 0 && entries[j - 1].name > key.name) {
      entries[j] = entries[j - 1];
      --j;
    }
    entries[j] = key;
  }
  return entries;
}

// Fires a compile-time error via `throw` if any two entries share a
// name. Pair with `sorted(...)` — the check only inspects adjacent
// entries.
template <class Value, std::size_t N>
consteval void requireUniqueNames(const std::array<Entry<Value>, N> &sorted) {
  for (std::size_t i = 1; i < N; ++i) {
    if (sorted[i - 1].name == sorted[i].name) {
      throw "duplicate name in lookup table";
    }
  }
  for (const auto &entry : sorted) {
    if (entry.name.empty()) {
      throw "empty name in lookup table";
    }
  }
}

// Binary search by name. Safe at runtime; constexpr-friendly.
template <class Value, std::size_t N>
[[nodiscard]] constexpr std::optional<Value>
find(const std::array<Entry<Value>, N> &sorted,
     std::string_view name) noexcept {
  std::size_t lo = 0;
  std::size_t hi = N;
  while (lo < hi) {
    const auto mid = lo + (hi - lo) / 2;
    if (sorted[mid].name < name) {
      lo = mid + 1;
    } else if (sorted[mid].name > name) {
      hi = mid;
    } else {
      return sorted[mid].value;
    }
  }
  return std::nullopt;
}

// Build a value→name table with SlotCount entries, using Indexer to
// place each entry. Empty slots indicate values not present in the
// input; the caller can decide whether that's an error.
template <std::size_t SlotCount, class Value, std::size_t N, class Indexer>
[[nodiscard]] consteval std::array<std::string_view, SlotCount>
reverseTable(const std::array<Entry<Value>, N> &entries, Indexer idx) {
  std::array<std::string_view, SlotCount> out{};
  for (const auto &entry : entries) {
    const auto slot = idx(entry.value);
    if (slot >= SlotCount) {
      throw "reverse-table slot out of range";
    }
    if (!out[slot].empty()) {
      throw "duplicate reverse-table slot";
    }
    out[slot] = entry.name;
  }
  return out;
}

// Variant of reverseTable that additionally demands every slot be
// filled. Use for enums where there's one entry per value.
template <std::size_t SlotCount, class Value, std::size_t N, class Indexer>
[[nodiscard]] consteval std::array<std::string_view, SlotCount>
reverseTableDense(const std::array<Entry<Value>, N> &entries, Indexer idx) {
  auto out = reverseTable<SlotCount>(entries, idx);
  for (const auto &name : out) {
    if (name.empty()) {
      throw "missing reverse-table slot";
    }
  }
  return out;
}

// Build a "known" bitmap of size SlotCount, slotted by the same
// Indexer. Used by channels to separate registered tags from
// fallthrough byte values.
template <std::size_t SlotCount, class Value, std::size_t N, class Indexer>
[[nodiscard]] consteval std::array<bool, SlotCount>
presenceTable(const std::array<Entry<Value>, N> &entries, Indexer idx) {
  std::array<bool, SlotCount> out{};
  for (const auto &entry : entries) {
    const auto slot = idx(entry.value);
    if (slot < SlotCount) {
      out[slot] = true;
    }
  }
  return out;
}

} // namespace PhantomLedger::lookup
