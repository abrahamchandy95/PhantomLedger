#pragma once

#include "phantomledger/primitives/utils/groups.hpp"
#include "phantomledger/spending/market/population/paydays.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::spending::simulator {

class PaydayIndex {
public:
  using Storage = primitives::utils::Csr<std::size_t, std::uint32_t>;

  PaydayIndex() = default;

  static PaydayIndex build(const market::population::Paydays &paydays,
                           std::uint32_t numDays);

  /// Persons receiving a paycheck on `dayIndex`
  [[nodiscard]] std::span<const std::uint32_t>
  personsOn(std::uint32_t dayIndex) const noexcept {
    if (dayIndex >= numDays_) {
      return {};
    }
    return storage_.rowOf(dayIndex);
  }

  [[nodiscard]] std::uint32_t numDays() const noexcept { return numDays_; }
  [[nodiscard]] std::size_t totalEntries() const noexcept {
    return storage_.totalEntries();
  }

private:
  std::uint32_t numDays_ = 0;
  Storage storage_;
};

inline PaydayIndex
PaydayIndex::build(const market::population::Paydays &paydays,
                   std::uint32_t numDays) {
  PaydayIndex idx;
  idx.numDays_ = numDays;

  auto &offsets = idx.storage_.offsetsMutable();
  auto &flat = idx.storage_.flatMutable();

  offsets.assign(static_cast<std::size_t>(numDays) + 1u, std::size_t{0});
  flat.clear();

  if (numDays == 0) {
    return idx;
  }

  const auto personCount = paydays.personCount();

  for (std::size_t p = 0; p < personCount; ++p) {
    const auto pv = paydays.personView(static_cast<std::uint32_t>(p));
    for (const std::uint32_t *it = pv.first; it != pv.last; ++it) {
      const auto day = *it;
      if (day < numDays) {
        ++offsets[day + 1u];
      }
    }
  }

  // --- Prefix sum into proper offsets ------------------------------------
  for (std::uint32_t d = 0; d < numDays; ++d) {
    offsets[d + 1u] += offsets[d];
  }

  flat.assign(offsets.back(), 0u);
  std::vector<std::size_t> cursor(offsets);

  for (std::size_t p = 0; p < personCount; ++p) {
    const auto pv = paydays.personView(static_cast<std::uint32_t>(p));
    for (const std::uint32_t *it = pv.first; it != pv.last; ++it) {
      const auto day = *it;
      if (day < numDays) {
        flat[cursor[day]++] = static_cast<std::uint32_t>(p);
      }
    }
  }

  return idx;
}

} // namespace PhantomLedger::spending::simulator
