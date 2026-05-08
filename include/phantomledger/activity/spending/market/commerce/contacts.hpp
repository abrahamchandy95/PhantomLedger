#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::spending::market::commerce {

class Contacts {
public:
  Contacts() = default;

  Contacts(std::uint32_t personCount, std::uint16_t degree)
      : personCount_(personCount), degree_(degree),
        flat_(static_cast<std::size_t>(personCount) * degree, 0u) {}

  [[nodiscard]] std::span<const std::uint32_t>
  rowOf(std::uint32_t personIndex) const noexcept {
    return std::span<const std::uint32_t>(flat_.data() + personIndex * degree_,
                                          degree_);
  }

  [[nodiscard]] std::span<std::uint32_t>
  rowOfMutable(std::uint32_t personIndex) noexcept {
    return std::span<std::uint32_t>(flat_.data() + personIndex * degree_,
                                    degree_);
  }

  [[nodiscard]] std::uint16_t degree() const noexcept { return degree_; }
  [[nodiscard]] std::uint32_t personCount() const noexcept {
    return personCount_;
  }

private:
  std::uint32_t personCount_ = 0;
  std::uint16_t degree_ = 0;
  std::vector<std::uint32_t> flat_;
};

} // namespace PhantomLedger::spending::market::commerce
