#pragma once

#include "phantomledger/activity/spending/routing/channel.hpp"
#include "phantomledger/taxonomies/clearing/types.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace PhantomLedger::diagnostics::spending {

namespace routing = ::PhantomLedger::activity::spending::routing;
namespace clearing = ::PhantomLedger::clearing;

inline constexpr std::size_t kSlotCount = routing::kSlotCount;
inline constexpr std::size_t kRejectReasonCount = clearing::kRejectReasonCount;

inline constexpr std::size_t kPersonaBuckets = 8;

inline constexpr std::size_t kMultiplierBins = 11;

// Count-distribution histogram: 0, 1, 2, 3-5, 6-10, 11-20, 21+
inline constexpr std::size_t kCountBins = 7;

inline constexpr std::size_t kSampleCapPerKind = 25;

enum class FailureStage : std::uint8_t {
  routeNullopt = 0,
  transferRejected = 1,
};

struct SampleFailure {
  std::uint32_t dayIndex;
  std::uint32_t personIndex;
  std::uint16_t personaBucket;
  routing::Slot slot;
  FailureStage stage;
  clearing::RejectReason reason;
  double liquidityMult;
  double availableToSpend;
};

struct DaySnapshot {
  std::uint32_t dayIndex;
  std::uint64_t txns;
  std::uint64_t routeNullopt;
  std::uint64_t transferRejected;
  std::uint64_t attempts;
  double avgLiquidityMult;
  double avgCountSampled;
};

class Stats {
public:
  static Stats &instance() noexcept;

  // Reset at the start of each Simulator::run().
  void reset() noexcept;

  // Hot-path accumulators.
  void recordAttempt(routing::Slot slot, std::uint16_t personaBucket) noexcept;

  void recordRouteNullopt(std::uint32_t dayIndex, std::uint32_t personIndex,
                          std::uint16_t personaBucket, routing::Slot slot,
                          double liquidityMult,
                          double availableToSpend) noexcept;

  void recordTransferRejected(std::uint32_t dayIndex, std::uint32_t personIndex,
                              std::uint16_t personaBucket, routing::Slot slot,
                              clearing::RejectReason reason,
                              double liquidityMult,
                              double availableToSpend) noexcept;

  void recordEmitted(routing::Slot slot, std::uint16_t personaBucket) noexcept;

  void recordCountSampled(std::uint32_t count) noexcept;

  void recordLiquidityMultiplier(double mult) noexcept;

  // Called once per simulated day from the day-driver, single
  // threaded, after the day's spender-emission loop completes.
  void recordDaySnapshot(std::uint32_t dayIndex) noexcept;

  // Structured dump at end of run.
  void dump() const noexcept;

private:
  Stats() = default;

  using AtomicU64 = std::atomic<std::uint64_t>;

  // Global counters
  AtomicU64 totalAttempts_{0};
  AtomicU64 totalRouteNullopt_{0};
  AtomicU64 totalTransferRejected_{0};
  AtomicU64 totalEmitted_{0};
  AtomicU64 totalCountSamples_{0};
  AtomicU64 totalCountSum_{0};
  std::atomic<double> liquiditySum_{0.0};
  AtomicU64 liquidityCount_{0};

  // Per-slot
  std::array<AtomicU64, kSlotCount> attemptsBySlot_{};
  std::array<AtomicU64, kSlotCount> routeNulloptBySlot_{};
  std::array<AtomicU64, kSlotCount> transferRejectedBySlot_{};
  std::array<AtomicU64, kSlotCount> emittedBySlot_{};

  // Per-rejection-reason
  std::array<AtomicU64, kRejectReasonCount> rejectsByReason_{};

  // Per-persona
  std::array<AtomicU64, kPersonaBuckets> attemptsByPersona_{};
  std::array<AtomicU64, kPersonaBuckets> emittedByPersona_{};

  // Histograms
  std::array<AtomicU64, kMultiplierBins> liquidityHistogram_{};
  std::array<AtomicU64, kCountBins> countHistogram_{};

  // Sample retention
  mutable std::mutex samplesMu_;
  std::array<std::vector<SampleFailure>, 2> samples_{};

  // Daily timeline
  mutable std::mutex daysMu_;
  std::vector<DaySnapshot> days_;
  AtomicU64 lastDayAttempts_{0};
  AtomicU64 lastDayRouteFail_{0};
  AtomicU64 lastDayXferFail_{0};
  AtomicU64 lastDayEmitted_{0};
  std::atomic<double> lastDayLiquiditySum_{0.0};
  AtomicU64 lastDayLiquidityCount_{0};
  AtomicU64 lastDayCountSum_{0};
  AtomicU64 lastDayCountSamples_{0};

  // Helpers
  [[nodiscard]] static std::size_t countBinFor(std::uint32_t count) noexcept;
  [[nodiscard]] static std::size_t multiplierBinFor(double multiplier) noexcept;
  static void pushSample(std::vector<SampleFailure> &bucket,
                         const SampleFailure &sample) noexcept;
};

} // namespace PhantomLedger::diagnostics::spending
