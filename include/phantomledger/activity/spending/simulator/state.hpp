#pragma once

#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/transactions/record.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace PhantomLedger::spending::simulator {

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

  // Reset a single person's counter to 0. Called by the day-driver
  // when this person receives a paycheck on the current day.
  void resetDaysSincePayday(std::uint32_t personIndex) noexcept {
    daysSincePayday_[personIndex] = 0;
  }

  // Set a single person's counter to an explicit value. Used by the
  // warm-start helper (see simulator/warm_start.hpp) to seed the
  // counter from each person's inferred cycle position at sim start.
  // After warm-start completes, the day-driver's bump/reset sequence
  // is the only writer.
  void setDaysSincePayday(std::uint32_t personIndex,
                          std::uint16_t value) noexcept {
    daysSincePayday_[personIndex] = value;
  }

  /// Bump every person's counter by one (saturating).
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

  void recordAccepted(std::uint32_t count) noexcept {
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
  // Pre-warm-start placeholder for every person's days-since-payday
  // counter. Callers are expected to invoke applyWarmStartDaysSincePayday
  // (simulator/warm_start.hpp) immediately after RunState construction
  // to overwrite this with each person's true cycle-position estimate.
  //
  // If warm-start is skipped (test paths, smoke runs without payday
  // data), every person reads 7 -- the stationary mean of Uniform(0,T)
  // for the default US biweekly cycle (T=14). This puts the population
  // at "average mid-cycle" rather than at maximum stress (the prior
  // value of 365) or maximum relief (0). Both extremes were wrong:
  // 365 implied "no paycheck in a year, max stress for everyone,"
  // 0 implied "everyone just got paid yesterday, max relief for
  // everyone." Neither matches the stationary distribution of a
  // population observed at an arbitrary moment in their pay cycles.
  static constexpr std::uint16_t kInitialDaysSincePayday = 7;

  std::vector<transactions::Transaction> txns_;
  std::vector<std::uint16_t> daysSincePayday_;
  std::size_t baseIdx_ = 0;
  std::atomic<std::uint64_t> remainingPersonDays_{0};
  std::atomic<double> remainingTargetTxns_{0.0};
};

struct ThreadLocalState {

  random::Rng rng;

  std::vector<transactions::Transaction> txns;

  explicit ThreadLocalState(random::Rng r) noexcept : rng(std::move(r)) {}

  ThreadLocalState(const ThreadLocalState &) = delete;
  ThreadLocalState &operator=(const ThreadLocalState &) = delete;
  ThreadLocalState(ThreadLocalState &&) noexcept = default;
  ThreadLocalState &operator=(ThreadLocalState &&) noexcept = default;
};

} // namespace PhantomLedger::spending::simulator
