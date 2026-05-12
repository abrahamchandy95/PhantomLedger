#include "phantomledger/diagnostics/spending_stats.hpp"
#include "phantomledger/diagnostics/logger.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

namespace PhantomLedger::diagnostics::spending {

namespace {

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
  case FailureStage::routeNullopt:
    return "route-nullopt";
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
    // retry
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
  totalRouteNullopt_ = 0;
  totalTransferRejected_ = 0;
  totalEmitted_ = 0;
  totalCountSamples_ = 0;
  totalCountSum_ = 0;
  liquiditySum_ = 0.0;
  liquidityCount_ = 0;

  for (auto &c : attemptsBySlot_)
    c = 0;
  for (auto &c : routeNulloptBySlot_)
    c = 0;
  for (auto &c : transferRejectedBySlot_)
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
    std::lock_guard lock(samplesMu_);
    samples_[0].clear();
    samples_[1].clear();
    samples_[0].reserve(kSampleCapPerKind);
    samples_[1].reserve(kSampleCapPerKind);
  }
  {
    std::lock_guard lock(daysMu_);
    days_.clear();
  }

  lastDayAttempts_ = 0;
  lastDayRouteFail_ = 0;
  lastDayXferFail_ = 0;
  lastDayEmitted_ = 0;
  lastDayLiquiditySum_ = 0.0;
  lastDayLiquidityCount_ = 0;
  lastDayCountSum_ = 0;
  lastDayCountSamples_ = 0;
}

std::size_t Stats::countBinFor(std::uint32_t count) noexcept {
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

std::size_t Stats::multiplierBinFor(double m) noexcept {
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

void Stats::recordAttempt(routing::Slot slot,
                          std::uint16_t personaBucket) noexcept {
  const auto i = static_cast<std::size_t>(slot);
  totalAttempts_.fetch_add(1, std::memory_order_relaxed);
  attemptsBySlot_[i].fetch_add(1, std::memory_order_relaxed);
  lastDayAttempts_.fetch_add(1, std::memory_order_relaxed);

  if (personaBucket < kPersonaBuckets) {
    attemptsByPersona_[personaBucket].fetch_add(1, std::memory_order_relaxed);
  }
}

void Stats::recordRouteNullopt(std::uint32_t dayIndex,
                               std::uint32_t personIndex,
                               std::uint16_t personaBucket, routing::Slot slot,
                               double liquidityMult,
                               double availableToSpend) noexcept {
  const auto i = static_cast<std::size_t>(slot);
  totalRouteNullopt_.fetch_add(1, std::memory_order_relaxed);
  routeNulloptBySlot_[i].fetch_add(1, std::memory_order_relaxed);
  lastDayRouteFail_.fetch_add(1, std::memory_order_relaxed);

  std::lock_guard lock(samplesMu_);
  pushSample(samples_[0], SampleFailure{
                              .dayIndex = dayIndex,
                              .personIndex = personIndex,
                              .personaBucket = personaBucket,
                              .slot = slot,
                              .stage = FailureStage::routeNullopt,
                              .reason = clearing::RejectReason::invalid,
                              .liquidityMult = liquidityMult,
                              .availableToSpend = availableToSpend,
                          });
}

void Stats::recordTransferRejected(std::uint32_t dayIndex,
                                   std::uint32_t personIndex,
                                   std::uint16_t personaBucket,
                                   routing::Slot slot,
                                   clearing::RejectReason reason,
                                   double liquidityMult,
                                   double availableToSpend) noexcept {
  const auto i = static_cast<std::size_t>(slot);
  const auto r = static_cast<std::size_t>(reason);
  totalTransferRejected_.fetch_add(1, std::memory_order_relaxed);
  transferRejectedBySlot_[i].fetch_add(1, std::memory_order_relaxed);
  if (r < rejectsByReason_.size()) {
    rejectsByReason_[r].fetch_add(1, std::memory_order_relaxed);
  }
  lastDayXferFail_.fetch_add(1, std::memory_order_relaxed);

  std::lock_guard lock(samplesMu_);
  pushSample(samples_[1], SampleFailure{
                              .dayIndex = dayIndex,
                              .personIndex = personIndex,
                              .personaBucket = personaBucket,
                              .slot = slot,
                              .stage = FailureStage::transferRejected,
                              .reason = reason,
                              .liquidityMult = liquidityMult,
                              .availableToSpend = availableToSpend,
                          });
}

void Stats::recordEmitted(routing::Slot slot,
                          std::uint16_t personaBucket) noexcept {
  const auto i = static_cast<std::size_t>(slot);
  totalEmitted_.fetch_add(1, std::memory_order_relaxed);
  emittedBySlot_[i].fetch_add(1, std::memory_order_relaxed);
  lastDayEmitted_.fetch_add(1, std::memory_order_relaxed);

  if (personaBucket < kPersonaBuckets) {
    emittedByPersona_[personaBucket].fetch_add(1, std::memory_order_relaxed);
  }
}

void Stats::recordCountSampled(std::uint32_t count) noexcept {
  totalCountSamples_.fetch_add(1, std::memory_order_relaxed);
  totalCountSum_.fetch_add(count, std::memory_order_relaxed);
  countHistogram_[countBinFor(count)].fetch_add(1, std::memory_order_relaxed);
  lastDayCountSum_.fetch_add(count, std::memory_order_relaxed);
  lastDayCountSamples_.fetch_add(1, std::memory_order_relaxed);
}

void Stats::recordLiquidityMultiplier(double mult) noexcept {
  fadd(liquiditySum_, mult);
  liquidityCount_.fetch_add(1, std::memory_order_relaxed);
  liquidityHistogram_[multiplierBinFor(mult)].fetch_add(
      1, std::memory_order_relaxed);
  fadd(lastDayLiquiditySum_, mult);
  lastDayLiquidityCount_.fetch_add(1, std::memory_order_relaxed);
}

void Stats::recordDaySnapshot(std::uint32_t dayIndex) noexcept {
  DaySnapshot snap{
      .dayIndex = dayIndex,
      .txns = lastDayEmitted_.exchange(0, std::memory_order_relaxed),
      .routeNullopt = lastDayRouteFail_.exchange(0, std::memory_order_relaxed),
      .transferRejected =
          lastDayXferFail_.exchange(0, std::memory_order_relaxed),
      .attempts = lastDayAttempts_.exchange(0, std::memory_order_relaxed),
      .avgLiquidityMult = 0.0,
      .avgCountSampled = 0.0,
  };

  const auto liqSum = fexchange(lastDayLiquiditySum_, 0.0);
  const auto liqCnt =
      lastDayLiquidityCount_.exchange(0, std::memory_order_relaxed);
  snap.avgLiquidityMult = safeDiv(liqSum, static_cast<double>(liqCnt));

  const auto cntSum = lastDayCountSum_.exchange(0, std::memory_order_relaxed);
  const auto cntCnt =
      lastDayCountSamples_.exchange(0, std::memory_order_relaxed);
  snap.avgCountSampled =
      safeDiv(static_cast<double>(cntSum), static_cast<double>(cntCnt));

  std::lock_guard lock(daysMu_);
  days_.push_back(snap);
}

void Stats::dump() const noexcept {
  if (!Logger::instance().enabled(Level::info, Topic::spending)) {
    return;
  }

  PL_LOG_INFO(spending, "===== Spending emission diagnostics =====");

  const auto totA = totalAttempts_.load();
  const auto totR = totalRouteNullopt_.load();
  const auto totT = totalTransferRejected_.load();
  const auto totE = totalEmitted_.load();

  PL_LOG_INFO(spending,
              "Totals: attempts=%llu route-nullopt=%llu (%.2f%%) "
              "xfer-rejected=%llu (%.2f%%) emitted=%llu (%.2f%%)",
              static_cast<unsigned long long>(totA),
              static_cast<unsigned long long>(totR), pct(totR, totA),
              static_cast<unsigned long long>(totT), pct(totT, totA),
              static_cast<unsigned long long>(totE), pct(totE, totA));

  const auto totCntSamples = totalCountSamples_.load();
  const auto totCntSum = totalCountSum_.load();
  const auto totLiqCnt = liquidityCount_.load();
  const auto totLiqSum = liquiditySum_.load();

  PL_LOG_INFO(spending,
              "Mean count sampled per spender-day: %.4f  "
              "Mean liquidity mult: %.4f",
              safeDiv(static_cast<double>(totCntSum),
                      static_cast<double>(totCntSamples)),
              safeDiv(totLiqSum, static_cast<double>(totLiqCnt)));

  PL_LOG_INFO(spending, "--- Per-slot breakdown ---");
  PL_LOG_INFO(spending, "%-10s %12s %12s %12s %12s %9s", "slot", "attempts",
              "route-fail", "xfer-fail", "emitted", "success%");
  for (std::size_t i = 0; i < kSlotCount; ++i) {
    const auto a = attemptsBySlot_[i].load();
    const auto r = routeNulloptBySlot_[i].load();
    const auto t = transferRejectedBySlot_[i].load();
    const auto e = emittedBySlot_[i].load();
    PL_LOG_INFO(spending, "%-10s %12llu %12llu %12llu %12llu %8.2f%%",
                slotName(static_cast<routing::Slot>(i)),
                static_cast<unsigned long long>(a),
                static_cast<unsigned long long>(r),
                static_cast<unsigned long long>(t),
                static_cast<unsigned long long>(e), pct(e, a));
  }

  PL_LOG_INFO(spending, "--- Transfer rejection reasons ---");
  for (std::size_t i = 0; i < kRejectReasonCount; ++i) {
    const auto c = rejectsByReason_[i].load();
    PL_LOG_INFO(spending, "  %-10s : %llu (%.2f%% of all xfer-rejects)",
                reasonName(static_cast<clearing::RejectReason>(i)),
                static_cast<unsigned long long>(c), pct(c, totT));
  }

  PL_LOG_INFO(spending, "--- Per-persona (bucket index) ---");
  PL_LOG_INFO(spending, "%-8s %12s %12s %9s", "bucket", "attempts", "emitted",
              "success%");
  for (std::size_t i = 0; i < kPersonaBuckets; ++i) {
    const auto a = attemptsByPersona_[i].load();
    const auto e = emittedByPersona_[i].load();
    if (a == 0 && e == 0)
      continue;
    PL_LOG_INFO(spending, "%-8zu %12llu %12llu %8.2f%%", i,
                static_cast<unsigned long long>(a),
                static_cast<unsigned long long>(e), pct(e, a));
  }

  PL_LOG_INFO(spending, "--- Liquidity-multiplier histogram (bin lower) ---");
  for (std::size_t i = 0; i < kMultiplierBins; ++i) {
    const auto c = liquidityHistogram_[i].load();
    PL_LOG_INFO(spending, "  %-5s : %llu (%.2f%%)", mulBinLabel(i),
                static_cast<unsigned long long>(c), pct(c, totLiqCnt));
  }

  PL_LOG_INFO(spending, "--- Sampled-count histogram ---");
  for (std::size_t i = 0; i < kCountBins; ++i) {
    const auto c = countHistogram_[i].load();
    PL_LOG_INFO(spending, "  %-6s : %llu (%.2f%%)", countBinLabel(i),
                static_cast<unsigned long long>(c), pct(c, totCntSamples));
  }

  {
    std::lock_guard lock(daysMu_);
    PL_LOG_INFO(spending, "--- Daily timeline (first 10, last 5) ---");
    PL_LOG_INFO(spending, "%-5s %10s %10s %10s %10s %10s %10s", "day",
                "attempts", "routeFail", "xferFail", "emitted", "avgMul",
                "avgCnt");
    const std::size_t n = days_.size();
    const std::size_t headLimit = std::min<std::size_t>(10, n);
    for (std::size_t i = 0; i < headLimit; ++i) {
      const auto &d = days_[i];
      PL_LOG_INFO(spending, "%-5u %10llu %10llu %10llu %10llu %10.3f %10.3f",
                  d.dayIndex, static_cast<unsigned long long>(d.attempts),
                  static_cast<unsigned long long>(d.routeNullopt),
                  static_cast<unsigned long long>(d.transferRejected),
                  static_cast<unsigned long long>(d.txns), d.avgLiquidityMult,
                  d.avgCountSampled);
    }
    if (n > 15) {
      PL_LOG_INFO(spending, "  ... %zu days elided ...", n - 15);
    }
    if (n > 10) {
      const std::size_t tailStart = (n > 5) ? n - 5 : 0;
      for (std::size_t i = std::max(tailStart, headLimit); i < n; ++i) {
        const auto &d = days_[i];
        PL_LOG_INFO(spending, "%-5u %10llu %10llu %10llu %10llu %10.3f %10.3f",
                    d.dayIndex, static_cast<unsigned long long>(d.attempts),
                    static_cast<unsigned long long>(d.routeNullopt),
                    static_cast<unsigned long long>(d.transferRejected),
                    static_cast<unsigned long long>(d.txns), d.avgLiquidityMult,
                    d.avgCountSampled);
      }
    }
  }

  {
    std::lock_guard lock(samplesMu_);
    PL_LOG_INFO(spending, "--- Sample failures (up to %zu of each kind) ---",
                kSampleCapPerKind);
    for (std::size_t k = 0; k < samples_.size(); ++k) {
      const auto &bucket = samples_[k];
      for (const auto &s : bucket) {
        PL_LOG_INFO(spending,
                    "  day=%u person=%u persona=%u slot=%-8s stage=%-13s "
                    "reason=%-8s liqMult=%.3f avail=%.2f",
                    s.dayIndex, s.personIndex,
                    static_cast<unsigned>(s.personaBucket), slotName(s.slot),
                    stageName(s.stage), reasonName(s.reason), s.liquidityMult,
                    s.availableToSpend);
      }
    }
  }

  PL_LOG_INFO(spending, "===== End diagnostics =====");
}

} // namespace PhantomLedger::diagnostics::spending
