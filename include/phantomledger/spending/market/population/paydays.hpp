#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::spending::market::population {

class Paydays {
public:
  struct PersonView {
    const std::uint32_t *first;
    const std::uint32_t *last;

    [[nodiscard]] bool contains(std::uint32_t dayIndex) const noexcept {
      return std::binary_search(first, last, dayIndex);
    }

    [[nodiscard]] std::size_t size() const noexcept {
      return static_cast<std::size_t>(last - first);
    }
  };

  Paydays() = default;

  Paydays(std::vector<std::uint32_t> offsets, std::vector<std::uint32_t> flat)
      : offsets_(std::move(offsets)), flat_(std::move(flat)) {}

  [[nodiscard]] PersonView
  personView(std::uint32_t personIndex) const noexcept {
    const auto begin = offsets_[personIndex];
    const auto end = offsets_[personIndex + 1];
    return PersonView{flat_.data() + begin, flat_.data() + end};
  }

  [[nodiscard]] bool isPayday(std::uint32_t personIndex,
                              std::uint32_t dayIndex) const noexcept {
    return personView(personIndex).contains(dayIndex);
  }

  [[nodiscard]] std::size_t personCount() const noexcept {
    return offsets_.empty() ? 0 : offsets_.size() - 1;
  }

private:
  std::vector<std::uint32_t> offsets_;
  std::vector<std::uint32_t> flat_;
};

} // namespace PhantomLedger::spending::market::population
