#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace PhantomLedger::primitives::utils {

template <typename Offset = std::uint32_t, typename Value = std::uint32_t>
class Csr {
public:
  using OffsetType = Offset;
  using ValueType = Value;

  Csr() = default;

  Csr(std::vector<Offset> offsets, std::vector<Value> flat) noexcept
      : offsets_(std::move(offsets)), flat_(std::move(flat)) {}

  [[nodiscard]] std::span<const Value> rowOf(std::uint32_t row) const noexcept {
    const auto begin = offsets_[row];
    const auto end = offsets_[row + 1];
    return std::span<const Value>(flat_.data() + begin,
                                  static_cast<std::size_t>(end - begin));
  }

  [[nodiscard]] std::span<Value> rowOfMutable(std::uint32_t row) noexcept {
    const auto begin = offsets_[row];
    const auto end = offsets_[row + 1];
    return std::span<Value>(flat_.data() + begin,
                            static_cast<std::size_t>(end - begin));
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
};

} // namespace PhantomLedger::primitives::utils
