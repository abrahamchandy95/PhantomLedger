#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace PhantomLedger::primitives::utils {

// A flat compressed-row container: `offsets_` bounds each row inside a
// single `flat_` array.
//
// VARIABLE-LENGTH ROWS (merchant-churn-2026-07). A row's slot span
// `[offsets[i], offsets[i+1])` is its CAPACITY; `counts_`, when populated,
// gives the number of live entries at the front of that span. `rowOf`
// returns only the live prefix, so a consumer sees a shorter row without
// knowing anything about the reservation behind it.
//
// This exists because growing a row the obvious way — inserting into
// `flat_` and bumping every later offset — is O(n) per insert AND
// invalidates every outstanding span. `src/activity/spending/dynamics/
// monthly/evolution.cpp` carried a TODO deferring the whole favourites
// add/drop pass on exactly that obstacle, which meant
// `math::evolution::evolveFavorites` was fully written, its config
// validated at startup, and NEVER CALLED: favourite merchant sets were
// frozen for every day of a 20-year run.
//
// With capacity plus a count, `pushBack` writes at `count++` and
// `swapRemove` moves the last live entry over the hole and decrements —
// both O(1), no reallocation, no offset arithmetic, spans stay valid.
//
// BACKWARD COMPATIBLE BY DESIGN: an instance built without counts (the
// two-argument constructor, which `Billers` still uses) reports each row's
// full span, so existing callers are bit-identical.
template <typename Offset = std::uint32_t, typename Value = std::uint32_t>
class Csr {
public:
  using OffsetType = Offset;
  using ValueType = Value;

  Csr() = default;

  Csr(std::vector<Offset> offsets, std::vector<Value> flat) noexcept
      : offsets_(std::move(offsets)), flat_(std::move(flat)) {}

  // Variable-length form: `counts[i]` live entries at the front of row i's
  // capacity span.
  Csr(std::vector<Offset> offsets, std::vector<Value> flat,
      std::vector<Offset> counts) noexcept
      : offsets_(std::move(offsets)), flat_(std::move(flat)),
        counts_(std::move(counts)) {}

  // Live length of row i. Falls back to the full capacity span when no
  // counts were supplied, which is what keeps fixed-length callers
  // unchanged.
  [[nodiscard]] std::size_t rowLength(std::uint32_t row) const noexcept {
    if (counts_.empty()) {
      return static_cast<std::size_t>(offsets_[row + 1] - offsets_[row]);
    }
    return static_cast<std::size_t>(counts_[row]);
  }

  [[nodiscard]] std::size_t rowCapacity(std::uint32_t row) const noexcept {
    return static_cast<std::size_t>(offsets_[row + 1] - offsets_[row]);
  }

  [[nodiscard]] std::span<const Value> rowOf(std::uint32_t row) const noexcept {
    return std::span<const Value>(flat_.data() + offsets_[row],
                                  rowLength(row));
  }

  [[nodiscard]] std::span<Value> rowOfMutable(std::uint32_t row) noexcept {
    return std::span<Value>(flat_.data() + offsets_[row], rowLength(row));
  }

  // Append to row i if it has spare capacity. Returns false when full —
  // the caller decides whether that is a no-op or an error. O(1).
  [[nodiscard]] bool pushBack(std::uint32_t row, Value value) noexcept {
    if (counts_.empty() || rowLength(row) >= rowCapacity(row)) {
      return false;
    }
    flat_[offsets_[row] + counts_[row]] = value;
    ++counts_[row];
    return true;
  }

  // Remove the entry at `slot` within row i by moving the LAST live entry
  // into its place. O(1), and it does not preserve order — which is correct
  // here: a favourite list is a set, and every consumer either scans it
  // whole or samples it uniformly.
  [[nodiscard]] bool swapRemove(std::uint32_t row, std::size_t slot) noexcept {
    const auto length = rowLength(row);
    if (counts_.empty() || slot >= length) {
      return false;
    }
    const auto base = offsets_[row];
    flat_[base + slot] = flat_[base + length - 1];
    --counts_[row];
    return true;
  }

  [[nodiscard]] const std::vector<Offset> &counts() const noexcept {
    return counts_;
  }

  [[nodiscard]] std::size_t rowCount() const noexcept {
    return offsets_.empty() ? 0 : offsets_.size() - 1;
  }

  [[nodiscard]] std::size_t totalEntries() const noexcept {
    return flat_.size();
  }

  [[nodiscard]] const std::vector<Offset> &offsets() const noexcept {
    return offsets_;
  }

  [[nodiscard]] const std::vector<Value> &flat() const noexcept {
    return flat_;
  }

  [[nodiscard]] std::vector<Value> &flatMutable() noexcept { return flat_; }

  [[nodiscard]] std::vector<Offset> &offsetsMutable() noexcept {
    return offsets_;
  }

private:
  std::vector<Offset> offsets_;
  std::vector<Value> flat_;
  // Empty => fixed-length rows (each row is its full capacity span).
  std::vector<Offset> counts_;
};

} // namespace PhantomLedger::primitives::utils
