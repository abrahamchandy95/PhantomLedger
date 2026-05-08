#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::spending::obligations {

class Burden {
public:
  Burden() = default;
  explicit Burden(std::vector<double> monthlyBurdens)
      : monthly_(std::move(monthlyBurdens)) {}

  [[nodiscard]] double monthlyAt(std::uint32_t personIndex) const noexcept {
    return monthly_[personIndex];
  }

  [[nodiscard]] std::span<const double> view() const noexcept {
    return std::span<const double>(monthly_.data(), monthly_.size());
  }

  [[nodiscard]] std::size_t size() const noexcept { return monthly_.size(); }

private:
  std::vector<double> monthly_;
};

} // namespace PhantomLedger::spending::obligations
