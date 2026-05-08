#pragma once

#include "phantomledger/primitives/utils/groups.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace PhantomLedger::spending::market::population {

// Per-person payday CSR.
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

  Paydays(std::vector<std::uint32_t> offsets,
          std::vector<std::uint32_t> flat) noexcept
      : csr_(std::move(offsets), std::move(flat)) {}

  [[nodiscard]] PersonView
  personView(std::uint32_t personIndex) const noexcept {
    const auto row = csr_.rowOf(personIndex);
    return PersonView{row.data(), row.data() + row.size()};
  }

  [[nodiscard]] bool isPayday(std::uint32_t personIndex,
                              std::uint32_t dayIndex) const noexcept {
    return personView(personIndex).contains(dayIndex);
  }

  [[nodiscard]] std::size_t personCount() const noexcept {
    return csr_.rowCount();
  }

  [[nodiscard]] std::size_t totalEntries() const noexcept {
    return csr_.totalEntries();
  }

private:
  primitives::utils::Csr<std::uint32_t, std::uint32_t> csr_;
};

} // namespace PhantomLedger::spending::market::population
