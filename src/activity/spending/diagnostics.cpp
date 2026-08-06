#include "phantomledger/activity/spending/diagnostics.hpp"

#include "phantomledger/diagnostics/logger.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

namespace PhantomLedger::activity::spending::diagnostics {

namespace {

namespace logging = ::PhantomLedger::diagnostics;

[[nodiscard]] const char *slotName(routing::Slot slot) noexcept {
  switch (slot) {
  case routing::Slot::merchant:
    return "merchant";
  case routing::Slot::bill:
    return "bill";
  case routing::Slot::p2p:
    return "p2p";
  case routing::Slot::externalUnknown:
    return "external";
  default:
    return "?";
  }
}

[[nodiscard]] const char *reasonName(clearing::RejectReason r) noexcept {
  switch (r) {
  case clearing::RejectReason::invalid:
    return "invalid";
  case clearing::RejectReason::unbooked:
    return "unbooked";
  case clearing::RejectReason::unfunded:
    return "unfunded";
  }
  return "?";
}

[[nodiscard]] const char *stageName(FailureStage stage) noexcept {
  switch (stage) {
  case FailureStage::routeFailed:
    return "route-failed";
  case FailureStage::transferRejected:
    return "xfer-rejected";
  }
  return "?";
}

[[nodiscard]] const char *countBinLabel(std::size_t i) noexcept {
  static constexpr std::array<const char *, kCountBins> kLabels = {
      "0", "1", "2", "3-5", "6-10", "11-20", "21+"};
  return i < kLabels.size() ? kLabels[i] : "?";
}

[[nodiscard]] const char *mulBinLabel(std::size_t i) noexcept {
  static constexpr std::array<const char *, kMultiplierBins> kLabels = {
      "0.0", "0.1", "0.2", "0.3", "0.4", "0.5",
      "0.6", "0.7", "0.8", "0.9", "1.0+"};
  return i < kLabels.size() ? kLabels[i] : "?";
}

void fadd(std::atomic<double> &a, double v) noexcept {
  double cur = a.load(std::memory_order_relaxed);
  while (!a.compare_exchange_weak(cur, cur + v, std::memory_order_relaxed)) {
  }
}

[[nodiscard]] double fexchange(std::atomic<double> &a, double v) noexcept {
  return a.exchange(v, std::memory_order_relaxed);
}

[[nodiscard]] double safeDiv(double num, double den) noexcept {
  return den > 0.0 ? num / den : 0.0;
}

[[nodiscard]] double pct(std::uint64_t num, std::uint64_t den) noexcept {
  return den > 0 ? 100.0 * static_cast<double>(num) / static_cast<double>(den)
                 : 0.0;
}

} // namespace

Stats &Stats::instance() noexcept {
  static Stats s;
  return s;
}

void Stats::reset() noexcept {
  totalAttempts_ = 0;
  totalRouteFailures_ = 0;
  totalTransferRejects_ = 0;
  totalEmitted_ = 0;
  totalCountSamples_ = 0;
  totalCountSum_ = 0;
  totalLiquiditySum_ = 0.0;
  totalLiquidityCount_ = 0;

  for (auto &c : attemptsBySlot_)
    c = 0;
  for (auto &c : routeFailuresBySlot_)
    c = 0;
  for (auto &c : transferRejectsBySlot_)
    c = 0;
  for (auto &c : emittedBySlot_)
    c = 0;
  for (auto &c : rejectsByReason_)
    c = 0;
  for (auto &c : attemptsByPersona_)
    c = 0;
  for (auto &c : emittedByPersona_)
    c = 0;
  for (auto &c : liquidityHistogram_)
    c = 0;
  for (auto &c : countHistogram_)
    c = 0;

  {
    std::lock_guard lock(samplesMutex_);
    samples_[0].clear();
    samples_[1].clear();
    samples_[0].reserve(kSampleCapPerKind);
    samples_[1].reserve(kSampleCapPerKind);
  }
  {
    std::lock_guard lock(daysMutex_);
    days_.clear();
  }

  lastDay_.attempts = 0;
  lastDay_.routeFailures = 0;
  lastDay_.transferRejects = 0;
  lastDay_.emitted = 0;
  lastDay_.liquiditySum = 0.0;
  lastDay_.liquidityCount = 0;
  lastDay_.countSum = 0;
  lastDay_.countSamples = 0;
}

std::size_t Stats::mapCountToBin(std::uint32_t count) noexcept {
  if (count == 0)
    return 0;
  if (count == 1)
    return 1;
  if (count == 2)
    return 2;
  if (count <= 5)
    return 3;
  if (count <= 10)
    return 4;
  if (count <= 20)
    return 5;
  return 6;
}

std::size_t Stats::mapLiquidityToBin(double m) noexcept {
  if (m >= 1.0)
    return kMultiplierBins - 1;
  if (m < 0.0)
    return 0;
  auto bin = static_cast<std::size_t>(m * 10.0);
  return std::min(bin, kMultiplierBins - 2);
}

void Stats::pushSample(std::vector<SampleFailure> &bucket,
                       const SampleFailure &sample) noexcept {
  if (bucket.size() < kSampleCapPerKind) {
    bucket.push_back(sample);
  }
}

void Stats::attempt(routing::Slot slot, std::uint16_t personaBucket) noexcept {
  const auto i = static_cast<std::size_t>(slot);
  totalAttempts_.fetch_add(1, std::memory_order_relaxed);
  attemptsBySlot_[i].fetch_add(1, std::memory_order_relaxed);
  lastDay_.attempts.fetch_add(1, std::memory_order_relaxed);

  if (personaBucket < kPersonaBuckets) {
    attemptsByPersona_[personaBucket].fetch_add(1, std::memory_order_relaxed);
  }
}

void Stats::routeFailure(const AttemptState &state) noexcept {
  const auto i = static_cast<std::size_t>(state.slot);
  totalRouteFailures_.fetch_add(1, std::memory_order_relaxed);
  routeFailuresBySlot_[i].fetch_add(1, std::memory_order_relaxed);
  lastDay_.routeFailures.fetch_add(1, std::memory_order_relaxed);

  std::lock_guard lock(samplesMutex_);
  pushSample(samples_[0], SampleFailure{
                              .state = state,
                              .stage = FailureStage::routeFailed,
                              .reason = clearing::RejectReason::invalid,
                          });
}

void Stats::transferFailure(const AttemptState &state,
                            clearing::RejectReason reason) noexcept {
  const auto i = static_cast<std::size_t>(state.slot);
  const auto r = static_cast<std::size_t>(reason);
  totalTransferRejects_.fetch_add(1, std::memory_order_relaxed);
  transferRejectsBySlot_[i].fetch_add(1, std::memory_order_relaxed);
  if (r < rejectsByReason_.size()) {
    rejectsByReason_[r].fetch_add(1, std::memory_order_relaxed);
  }
  lastDay_.transferRejects.fetch_add(1, std::memory_order_relaxed);

  std::lock_guard lock(samplesMutex_);
  pushSample(samples_[1], SampleFailure{
                              .state = state,
                              .stage = FailureStage::transferRejected,
                              .reason = reason,
                          });
}

void Stats::emitted(routing::Slot slot, std::uint16_t personaBucket) noexcept {
  const auto i = static_cast<std::size_t>(slot);
  totalEmitted_.fetch_add(1, std::memory_order_relaxed);
  emittedBySlot_[i].fetch_add(1, std::memory_order_relaxed);
  lastDay_.emitted.fetch_add(1, std::memory_order_relaxed);

  if (personaBucket < kPersonaBuckets) {
    emittedByPersona_[personaBucket].fetch_add(1, std::memory_order_relaxed);
  }
}

void Stats::countSample(std::uint32_t count) noexcept {
  totalCountSamples_.fetch_add(1, std::memory_order_relaxed);
  totalCountSum_.fetch_add(count, std::memory_order_relaxed);
  countHistogram_[mapCountToBin(count)].fetch_add(1, std::memory_order_relaxed);
  lastDay_.countSum.fetch_add(count, std::memory_order_relaxed);
  lastDay_.countSamples.fetch_add(1, std::memory_order_relaxed);
}

void Stats::liquidityMultiplier(double mult) noexcept {
  fadd(totalLiquiditySum_, mult);
  totalLiquidityCount_.fetch_add(1, std::memory_order_relaxed);
  liquidityHistogram_[mapLiquidityToBin(mult)].fetch_add(
      1, std::memory_order_relaxed);
  fadd(lastDay_.liquiditySum, mult);
  lastDay_.liquidityCount.fetch_add(1, std::memory_order_relaxed);
}

void Stats::snapshotDay(std::uint32_t dayIndex) noexcept {
  DaySnapshot snap{
      .dayIndex = dayIndex,
      .txns = lastDay_.emitted.exchange(0, std::memory_order_relaxed),
      .routeFailures =
          lastDay_.routeFailures.exchange(0, std::memory_order_relaxed),
      .transferRejects =
          lastDay_.transferRejects.exchange(0, std::memory_order_relaxed),
      .attempts = lastDay_.attempts.exchange(0, std::memory_order_relaxed),
      .avgLiquidityMult = 0.0,
      .avgCountSampled = 0.0,
  };

  const auto liqSum = fexchange(lastDay_.liquiditySum, 0.0);
  const auto liqCnt =
      lastDay_.liquidityCount.exchange(0, std::memory_order_relaxed);
  snap.avgLiquidityMult = safeDiv(liqSum, static_cast<double>(liqCnt));

  const auto cntSum = lastDay_.countSum.exchange(0, std::memory_order_relaxed);
  const auto cntCnt =
      lastDay_.countSamples.exchange(0, std::memory_order_relaxed);
  snap.avgCountSampled =
      safeDiv(static_cast<double>(cntSum), static_cast<double>(cntCnt));

  std::lock_guard lock(daysMutex_);
  days_.push_back(snap);
}

void Stats::dump() const noexcept {
  if (!logging::Logger::instance().enabled(logging::Level::info,
                                           logging::Topic::spending)) {
    return;
  }

  PL_LOG_INFO(spending, "===== Spending emission diagnostics =====");

  const auto totA = totalAttempts_.load();
  const auto totR = totalRouteFailures_.load();
  const auto totT = totalTransferRejects_.load();
  const auto totE = totalEmitted_.load();

  PL_LOG_INFO(spending,
              "Totals: attempts={} route-nullopt={} ({:.2f}%) "
              "xfer-rejected={} ({:.2f}%) emitted={} ({:.2f}%)",
              totA, totR, pct(totR, totA), totT, pct(totT, totA), totE,
              pct(totE, totA));

  const auto totCntSamples = totalCountSamples_.load();
  const auto totCntSum = totalCountSum_.load();
  const auto totLiqCnt = totalLiquidityCount_.load();
  const auto totLiqSum = totalLiquiditySum_.load();

  PL_LOG_INFO(
      spending,
      "Mean count sampled per spender-day: {:.4f}  Mean liquidity mult: {:.4f}",
      safeDiv(static_cast<double>(totCntSum),
              static_cast<double>(totCntSamples)),
      safeDiv(totLiqSum, static_cast<double>(totLiqCnt)));

  PL_LOG_INFO(spending, "--- Per-slot breakdown ---");
  PL_LOG_INFO(spending, "{:<10} {:>12} {:>12} {:>12} {:>12} {:>9}", "slot",
              "attempts", "route-fail", "xfer-fail", "emitted", "success%");
  for (std::size_t i = 0; i < kSlotCount; ++i) {
    const auto a = attemptsBySlot_[i].load();
    const auto r = routeFailuresBySlot_[i].load();
    const auto t = transferRejectsBySlot_[i].load();
    const auto e = emittedBySlot_[i].load();
    PL_LOG_INFO(spending, "{:<10} {:>12} {:>12} {:>12} {:>12} {:>8.2f}%",
                slotName(static_cast<routing::Slot>(i)), a, r, t, e, pct(e, a));
  }

  PL_LOG_INFO(spending, "--- Transfer rejection reasons ---");
  for (std::size_t i = 0; i < kRejectReasonCount; ++i) {
    const auto c = rejectsByReason_[i].load();
    PL_LOG_INFO(spending, "  {:<10} : {} ({:.2f}% of all xfer-rejects)",
                reasonName(static_cast<clearing::RejectReason>(i)), c,
                pct(c, totT));
  }

  PL_LOG_INFO(spending, "--- Per-persona (bucket index) ---");
  PL_LOG_INFO(spending, "{:<8} {:>12} {:>12} {:>9}", "bucket", "attempts",
              "emitted", "success%");
  for (std::size_t i = 0; i < kPersonaBuckets; ++i) {
    const auto a = attemptsByPersona_[i].load();
    const auto e = emittedByPersona_[i].load();
    if (a == 0 && e == 0)
      continue;
    PL_LOG_INFO(spending, "{:<8} {:>12} {:>12} {:>8.2f}%", i, a, e, pct(e, a));
  }

  PL_LOG_INFO(spending, "--- Liquidity-multiplier histogram (bin lower) ---");
  for (std::size_t i = 0; i < kMultiplierBins; ++i) {
    const auto c = liquidityHistogram_[i].load();
    PL_LOG_INFO(spending, "  {:<5} : {} ({:.2f}%)", mulBinLabel(i), c,
                pct(c, totLiqCnt));
  }

  PL_LOG_INFO(spending, "--- Sampled-count histogram ---");
  for (std::size_t i = 0; i < kCountBins; ++i) {
    const auto c = countHistogram_[i].load();
    PL_LOG_INFO(spending, "  {:<6} : {} ({:.2f}%)", countBinLabel(i), c,
                pct(c, totCntSamples));
  }

  {
    std::lock_guard lock(daysMutex_);
    PL_LOG_INFO(spending, "--- Daily timeline (first 10, last 5) ---");
    PL_LOG_INFO(spending, "{:<5} {:>10} {:>10} {:>10} {:>10} {:>10} {:>10}",
                "day", "attempts", "routeFail", "xferFail", "emitted", "avgMul",
                "avgCnt");

    const std::size_t n = days_.size();
    const std::size_t headLimit = std::min<std::size_t>(10, n);
    for (std::size_t i = 0; i < headLimit; ++i) {
      const auto &d = days_[i];
      PL_LOG_INFO(spending,
                  "{:<5} {:>10} {:>10} {:>10} {:>10} {:>10.3f} {:>10.3f}",
                  d.dayIndex, d.attempts, d.routeFailures, d.transferRejects,
                  d.txns, d.avgLiquidityMult, d.avgCountSampled);
    }
    if (n > 15) {
      PL_LOG_INFO(spending, "  ... {} days elided ...", n - 15);
    }
    if (n > 10) {
      const std::size_t tailStart = (n > 5) ? n - 5 : 0;
      for (std::size_t i = std::max(tailStart, headLimit); i < n; ++i) {
        const auto &d = days_[i];
        PL_LOG_INFO(spending,
                    "{:<5} {:>10} {:>10} {:>10} {:>10} {:>10.3f} {:>10.3f}",
                    d.dayIndex, d.attempts, d.routeFailures, d.transferRejects,
                    d.txns, d.avgLiquidityMult, d.avgCountSampled);
      }
    }
  }

  {
    std::lock_guard lock(samplesMutex_);
    PL_LOG_INFO(spending, "--- Sample failures (up to {} of each kind) ---",
                kSampleCapPerKind);
    for (std::size_t k = 0; k < samples_.size(); ++k) {
      const auto &bucket = samples_[k];
      for (const auto &s : bucket) {
        PL_LOG_INFO(spending,
                    "  day={} person={} persona={} slot={:<8} stage={:<13} "
                    "reason={:<8} liqMult={:.3f} avail={:.2f}",
                    s.state.dayIndex, s.state.personIndex,
                    s.state.personaBucket, slotName(s.state.slot),
                    stageName(s.stage), reasonName(s.reason),
                    s.state.liquidityMult, s.state.availableToSpend);
      }
    }
  }

  PL_LOG_INFO(spending, "===== End diagnostics =====");
}

} // namespace PhantomLedger::activity::spending::diagnostics
