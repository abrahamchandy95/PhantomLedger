#pragma once

#include "phantomledger/transactions/record.hpp"

#include <climits>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace PhantomLedger::spending::simulator {

class RunState {
public:
  RunState() = default;

  RunState(std::uint32_t personCount, std::size_t txnCapacity)
      : daysSincePayday_(personCount, kInitialDaysSincePayday) {
    txns_.reserve(txnCapacity);
  }

  [[nodiscard]] std::vector<transactions::Transaction> &txns() noexcept {
    return txns_;
  }
  [[nodiscard]] const std::vector<transactions::Transaction> &
  txns() const noexcept {
    return txns_;
  }

  [[nodiscard]] std::uint16_t
  daysSincePayday(std::uint32_t personIndex) const noexcept {
    return daysSincePayday_[personIndex];
  }

  void resetDaysSincePayday(std::uint32_t personIndex) noexcept {
    daysSincePayday_[personIndex] = 0;
  }

  void incrementDaysSincePayday(std::uint32_t personIndex) noexcept {
    auto &v = daysSincePayday_[personIndex];
    if (v < UINT16_MAX) {
      ++v;
    }
  }

  [[nodiscard]] std::size_t baseIdx() const noexcept { return baseIdx_; }
  void setBaseIdx(std::size_t idx) noexcept { baseIdx_ = idx; }

  [[nodiscard]] std::uint64_t processedPersonDays() const noexcept {
    return processedPersonDays_;
  }
  void incrementProcessedPersonDays() noexcept { ++processedPersonDays_; }

private:
  /// Initialized to 365 so the first pre-payday day already
  /// triggers the stress region of the liquidity multiplier curve
  /// (matches Python `RunState(days_since_payday=[365] * n)`).
  static constexpr std::uint16_t kInitialDaysSincePayday = 365;

  std::vector<transactions::Transaction> txns_;
  std::vector<std::uint16_t> daysSincePayday_;
  std::size_t baseIdx_ = 0;
  std::uint64_t processedPersonDays_ = 0;
};

} // namespace PhantomLedger::spending::simulator
