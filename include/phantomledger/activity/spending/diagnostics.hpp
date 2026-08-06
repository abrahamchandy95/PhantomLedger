#pragma once

#include "phantomledger/activity/spending/routing/channel.hpp"
#include "phantomledger/taxonomies/clearing/types.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

/*
  Spending Emission Diagnostics
  Tracks hot-path counters, histograms, and failure samples for the spending
  simulator. Dumps a structured report at the end of the run.
 */

namespace PhantomLedger::activity::spending::diagnostics {

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
  routeFailed = 0,
  transferRejected = 1,
};

/*
  Snapshot of a spender's financial and temporal state during an emission
  attempt.
 */
struct AttemptState {
  std::uint32_t dayIndex;
  std::uint32_t personIndex;
  std::uint16_t personaBucket;
  routing::Slot slot;
  double liquidityMult;
  double availableToSpend;
};

struct SampleFailure {
  AttemptState state;
  FailureStage stage;
  clearing::RejectReason reason;
};

struct DaySnapshot {
  std::uint32_t dayIndex;
  std::uint64_t txns;
  std::uint64_t routeFailures;
  std::uint64_t transferRejects;
  std::uint64_t attempts;
  double avgLiquidityMult;
  double avgCountSampled;
};

class Stats {
public:
  static Stats &instance() noexcept;

  void reset() noexcept;

  void attempt(routing::Slot slot, std::uint16_t personaBucket) noexcept;

  void routeFailure(const AttemptState &state) noexcept;

  void transferFailure(const AttemptState &state,
                       clearing::RejectReason reason) noexcept;

  void emitted(routing::Slot slot, std::uint16_t personaBucket) noexcept;

  void countSample(std::uint32_t count) noexcept;

  void liquidityMultiplier(double mult) noexcept;

  void snapshotDay(std::uint32_t dayIndex) noexcept;

  void dump() const noexcept;

private:
  Stats() = default;

  using AtomicU64 = std::atomic<std::uint64_t>;

  AtomicU64 totalAttempts_{0};
  AtomicU64 totalRouteFailures_{0};
  AtomicU64 totalTransferRejects_{0};
  AtomicU64 totalEmitted_{0};
  AtomicU64 totalCountSamples_{0};
  AtomicU64 totalCountSum_{0};
  std::atomic<double> totalLiquiditySum_{0.0};
  AtomicU64 totalLiquidityCount_{0};

  std::array<AtomicU64, kSlotCount> attemptsBySlot_{};
  std::array<AtomicU64, kSlotCount> routeFailuresBySlot_{};
  std::array<AtomicU64, kSlotCount> transferRejectsBySlot_{};
  std::array<AtomicU64, kSlotCount> emittedBySlot_{};

  std::array<AtomicU64, kRejectReasonCount> rejectsByReason_{};

  std::array<AtomicU64, kPersonaBuckets> attemptsByPersona_{};
  std::array<AtomicU64, kPersonaBuckets> emittedByPersona_{};

  std::array<AtomicU64, kMultiplierBins> liquidityHistogram_{};
  std::array<AtomicU64, kCountBins> countHistogram_{};

  mutable std::mutex samplesMutex_;
  std::array<std::vector<SampleFailure>, 2> samples_{};

  mutable std::mutex daysMutex_;
  std::vector<DaySnapshot> days_;

  /* Hot-path scratchpad for the day currently being simulated */
  struct LastDay {
    AtomicU64 attempts{0};
    AtomicU64 routeFailures{0};
    AtomicU64 transferRejects{0};
    AtomicU64 emitted{0};
    std::atomic<double> liquiditySum{0.0};
    AtomicU64 liquidityCount{0};
    AtomicU64 countSum{0};
    AtomicU64 countSamples{0};
  } lastDay_;

  [[nodiscard]] static std::size_t mapCountToBin(std::uint32_t count) noexcept;
  [[nodiscard]] static std::size_t
  mapLiquidityToBin(double multiplier) noexcept;
  static void pushSample(std::vector<SampleFailure> &bucket,
                         const SampleFailure &sample) noexcept;
};

} // namespace PhantomLedger::activity::spending::diagnostics
