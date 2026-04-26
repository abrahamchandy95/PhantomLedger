#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::spending::market::commerce {

class CsrIndex {
public:
  CsrIndex() = default;

  CsrIndex(std::vector<std::uint32_t> offsets, std::vector<std::uint32_t> flat)
      : offsets_(std::move(offsets)), flat_(std::move(flat)) {}

  [[nodiscard]] std::span<const std::uint32_t>
  rowOf(std::uint32_t personIndex) const noexcept {
    const auto begin = offsets_[personIndex];
    const auto end = offsets_[personIndex + 1];
    return std::span<const std::uint32_t>(flat_.data() + begin, end - begin);
  }

  /// Mutable view used during monthly evolution.
  [[nodiscard]] std::span<std::uint32_t>
  rowOfMutable(std::uint32_t personIndex) noexcept {
    const auto begin = offsets_[personIndex];
    const auto end = offsets_[personIndex + 1];
    return std::span<std::uint32_t>(flat_.data() + begin, end - begin);
  }

  [[nodiscard]] std::size_t personCount() const noexcept {
    return offsets_.empty() ? 0 : offsets_.size() - 1;
  }

  [[nodiscard]] std::size_t totalEntries() const noexcept {
    return flat_.size();
  }

  [[nodiscard]] const std::vector<std::uint32_t> &offsets() const noexcept {
    return offsets_;
  }

  [[nodiscard]] const std::vector<std::uint32_t> &flat() const noexcept {
    return flat_;
  }

  [[nodiscard]] std::vector<std::uint32_t> &flatMutable() noexcept {
    return flat_;
  }

private:
  std::vector<std::uint32_t> offsets_; // size == personCount + 1
  std::vector<std::uint32_t> flat_;    // size == offsets_.back()
};

using Favorites = CsrIndex;
using Billers = CsrIndex;

} // namespace PhantomLedger::spending::market::commerce
