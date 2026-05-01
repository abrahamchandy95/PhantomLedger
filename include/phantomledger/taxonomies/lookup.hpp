#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace PhantomLedger::lookup {

template <class Value> struct Entry {
  std::string_view name;
  Value value;
};

template <class Value, std::size_t N>
[[nodiscard]] consteval std::array<Entry<Value>, N>
sorted(std::array<Entry<Value>, N> entries) {
  for (std::size_t index = 1; index < N; ++index) {
    const auto key = entries[index];

    auto cursor = index;
    while (cursor > 0 && entries[cursor - 1].name > key.name) {
      entries[cursor] = entries[cursor - 1];
      --cursor;
    }

    entries[cursor] = key;
  }

  return entries;
}

template <class Value, std::size_t N>
consteval void requireUniqueNames(const std::array<Entry<Value>, N> &entries) {
  for (const auto &entry : entries) {
    if (entry.name.empty()) {
      throw "empty name in lookup table";
    }
  }

  for (std::size_t index = 1; index < N; ++index) {
    if (entries[index - 1].name > entries[index].name) {
      throw "lookup table must be sorted before validation";
    }

    if (entries[index - 1].name == entries[index].name) {
      throw "duplicate name in lookup table";
    }
  }
}

template <class Value, std::size_t N>
[[nodiscard]] constexpr std::optional<Value>
find(const std::array<Entry<Value>, N> &entries,
     std::string_view name) noexcept {
  std::size_t low = 0;
  std::size_t high = N;

  while (low < high) {
    const auto middle = low + (high - low) / 2;

    if (entries[middle].name < name) {
      low = middle + 1;
    } else if (entries[middle].name > name) {
      high = middle;
    } else {
      return entries[middle].value;
    }
  }

  return std::nullopt;
}

template <std::size_t IndexCount, class Value, std::size_t N, class Indexer>
[[nodiscard]] consteval std::array<std::string_view, IndexCount>
reverseTable(const std::array<Entry<Value>, N> &entries, Indexer indexOf) {
  std::array<std::string_view, IndexCount> out{};

  for (const auto &entry : entries) {
    const auto idx = static_cast<std::size_t>(indexOf(entry.value));

    if (idx >= IndexCount) {
      throw "reverse-table slot out of range";
    }

    if (!out[idx].empty()) {
      throw "duplicate reverse-table slot";
    }

    out[idx] = entry.name;
  }

  return out;
}

template <std::size_t IndexCount, class Value, std::size_t N, class Indexer>
[[nodiscard]] consteval std::array<std::string_view, IndexCount>
reverseTableDense(const std::array<Entry<Value>, N> &entries, Indexer indexOf) {
  auto out = reverseTable<IndexCount>(entries, indexOf);

  for (const auto &name : out) {
    if (name.empty()) {
      throw "missing reverse-table slot";
    }
  }

  return out;
}

template <std::size_t IndexCount, class Value, std::size_t N, class Indexer>
[[nodiscard]] consteval std::array<bool, IndexCount>
presenceTable(const std::array<Entry<Value>, N> &entries, Indexer indexOf) {
  std::array<bool, IndexCount> out{};

  for (const auto &entry : entries) {
    const auto idx = static_cast<std::size_t>(indexOf(entry.value));

    if (idx >= IndexCount) {
      throw "presence-table slot out of range";
    }

    out[idx] = true;
  }

  return out;
}

} // namespace PhantomLedger::lookup
