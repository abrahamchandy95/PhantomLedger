#pragma once

#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace PhantomLedger::activity::spending::simulator {

class RunState {
public:
  RunState() = default;

  RunState(std::uint32_t personCount, std::size_t txnCapacity,
           std::uint64_t totalPersonDays, double targetTotalTxns)
      : daysSincePayday_(personCount, kInitialDaysSincePayday),
        remainingPersonDays_(totalPersonDays),
        remainingTargetTxns_(targetTotalTxns) {
    txns_.reserve(txnCapacity);
  }

  // ----------------------------------------------------------------- txns
  [[nodiscard]] std::vector<transactions::Transaction> &txns() noexcept {
    return txns_;
  }
  [[nodiscard]] const std::vector<transactions::Transaction> &
  txns() const noexcept {
    return txns_;
  }

  // ---------------------------------------------------- days_since_payday
  [[nodiscard]] std::uint16_t
  daysSincePayday(std::uint32_t personIndex) const noexcept {
    return daysSincePayday_[personIndex];
  }

  void resetDaysSincePayday(std::uint32_t personIndex) noexcept {
    daysSincePayday_[personIndex] = 0;
  }

  // Set a single person's counter to an explicit value.
  void setDaysSincePayday(std::uint32_t personIndex,
                          std::uint16_t value) noexcept {
    daysSincePayday_[personIndex] = value;
  }

  /* Bump every person's counter by one (saturating). */
  void bumpAllDaysSincePayday() noexcept {
    constexpr auto kCap = std::numeric_limits<std::uint16_t>::max();
    for (auto &v : daysSincePayday_) {
      v = static_cast<std::uint16_t>(v + (v < kCap ? 1 : 0));
    }
  }

  // --------------------------------------------------------- base txn idx
  [[nodiscard]] std::size_t baseIdx() const noexcept { return baseIdx_; }
  void setBaseIdx(std::size_t idx) noexcept { baseIdx_ = idx; }

  [[nodiscard]] std::uint64_t remainingPersonDays() const noexcept {
    return remainingPersonDays_.load(std::memory_order_relaxed);
  }

  void consumeOnePersonDay() noexcept {
    auto cur = remainingPersonDays_.load(std::memory_order_relaxed);
    while (cur > 0 && !remainingPersonDays_.compare_exchange_weak(
                          cur, cur - 1, std::memory_order_relaxed)) {
      // retry
    }
  }

  [[nodiscard]] double remainingTargetTxns() const noexcept {
    return remainingTargetTxns_.load(std::memory_order_relaxed);
  }

  void consumeAccepted(std::uint32_t count) noexcept {
    if (count == 0) {
      return;
    }
    const double c = static_cast<double>(count);
    auto cur = remainingTargetTxns_.load(std::memory_order_relaxed);
    while (true) {
      const double next = cur > c ? cur - c : 0.0;
      if (remainingTargetTxns_.compare_exchange_weak(
              cur, next, std::memory_order_relaxed)) {
        break;
      }
    }
  }

private:
  static constexpr std::uint16_t kInitialDaysSincePayday = 7;

  std::vector<transactions::Transaction> txns_;
  std::vector<std::uint16_t> daysSincePayday_;
  std::size_t baseIdx_ = 0;
  std::atomic<std::uint64_t> remainingPersonDays_{0};
  std::atomic<double> remainingTargetTxns_{0.0};
};

/* One worker's reusable emission buffers, kept alive across days so the hot
 * path does not reallocate. Non-copyable: each worker owns exactly one. */
struct ThreadScratch {
  std::vector<transactions::Transaction> txns;
  std::vector<clearing::Ledger::Posting> postings;

  ThreadScratch() = default;

  ThreadScratch(const ThreadScratch &) = delete;
  ThreadScratch &operator=(const ThreadScratch &) = delete;
  ThreadScratch(ThreadScratch &&) noexcept = default;
  ThreadScratch &operator=(ThreadScratch &&) noexcept = default;
};

} // namespace PhantomLedger::activity::spending::simulator
