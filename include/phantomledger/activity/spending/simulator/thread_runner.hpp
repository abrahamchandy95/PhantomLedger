#pragma once

#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

namespace PhantomLedger::spending::simulator {

struct PartitionRange {
  std::size_t begin = 0;
  std::size_t end = 0;
  [[nodiscard]] std::size_t size() const noexcept { return end - begin; }
};

[[nodiscard]] inline PartitionRange
partitionRange(std::size_t total, std::uint32_t threadCount,
               std::uint32_t threadIdx) noexcept {
  if (threadCount == 0) {
    return {0, 0};
  }
  const std::size_t base = total / threadCount;
  const std::size_t rem = total % threadCount;
  // Spread the `rem` extra items over the first `rem` threads. Keeps
  // chunks within ±1 of each other regardless of how `total` divides.
  const std::size_t begin =
      threadIdx * base + (threadIdx < rem ? threadIdx : rem);
  const std::size_t extra = (threadIdx < rem) ? 1 : 0;
  const std::size_t end = begin + base + extra;
  return {begin, end};
}

/// Spawn `threadCount` workers, each running `body(threadIdx)`, and
/// join all of them before returning.

template <typename Body>
inline void runParallel(std::uint32_t threadCount, Body &&body) {
  if (threadCount <= 1) {
    body(static_cast<std::uint32_t>(0));
    return;
  }
  std::vector<std::thread> workers;
  workers.reserve(threadCount);
  for (std::uint32_t t = 0; t < threadCount; ++t) {
    workers.emplace_back([t, &body]() { body(t); });
  }
  for (auto &w : workers) {
    w.join();
  }
}

} // namespace PhantomLedger::spending::simulator
