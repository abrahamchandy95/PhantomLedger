#include "phantomledger/synth/infra/attackers.hpp"

#include "phantomledger/entities/infra/random_ips.hpp"
#include "phantomledger/primitives/random/distributions/lognormal.hpp"
#include "phantomledger/synth/infra/tenure_table.hpp"
#include "phantomledger/synth/infra/timeline.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace PhantomLedger::synth::infra::attackers {

namespace {

namespace dist = ::PhantomLedger::probability::distributions;

using AttackerOperator = ::PhantomLedger::infra::AttackerOperator;
using Tenure = ::PhantomLedger::infra::Tenure;

[[nodiscard]] std::uint32_t
operatorCountFor(const AssignmentRules &rules, std::uint32_t population,
                 std::int32_t windowDays) noexcept {
  if (population == 0 || windowDays <= 0) {
    return 0;
  }
  const auto days = static_cast<double>(windowDays);
  const auto years = days / 365.25;

  const auto byPopulation = rules.operatorsPerTenKPersonYears *
                            (static_cast<double>(population) / 10'000.0) *
                            years;

  // THE CONCURRENCY FLOOR, converted to a count.
  //
  // A campaign's start is drawn over [-L, W) so that coverage is UNIFORM
  // across the window (see the generator below for why it may not be
  // drawn over [0, W)). Over that support each of the W window days is
  // covered by exactly L of the W+L start positions, so expected
  // in-window coverage per operator is L*W/(W+L) and mean concurrency is
  //
  //     count * L / (W + L)
  //
  // Inverting that is what makes the floor mean what it says. The naive
  // `C * W / L` form — right only if every campaign sat wholly inside the
  // window — under-delivered the floor by ~20% at these shapes, and the
  // gate's own B' sub-gate is what caught it.
  //
  // L here is the rule's MEASURED effective length, not the length
  // distribution's analytic mean; see `effectiveCampaignDays` for why the
  // analytic value under-delivered the floor a second time.
  const auto meanLength = rules.effectiveCampaignDays;
  const auto byCoverage =
      rules.minConcurrentOperators * (days + meanLength) / meanLength;

  // At least one operator whenever there is a population and a window at
  // all: the unauthorized rails are ring-free and exist at every
  // population (test_fraud_low_population), so an empty attacker pool
  // would silence exactly the fraud family that is supposed to survive a
  // small world.
  const auto raw = std::max({1.0, byPopulation, byCoverage});
  const auto rounded = static_cast<std::uint64_t>(std::llround(raw));
  return static_cast<std::uint32_t>(
      std::min<std::uint64_t>(rounded, rules.maxOperators));
}

// Pareto(alpha) via inverse transform, clamped. One draw.
[[nodiscard]] double sampleWeight(random::Rng &rng,
                                  const AssignmentRules &rules) {
  // u in (0, 1]; guard the open end so the transform cannot divide by
  // zero on a 0.0 draw.
  const double u = std::max(1.0e-12, 1.0 - rng.nextDouble());
  const double w = std::pow(u, -1.0 / rules.weightAlpha);
  return std::clamp(w, 1.0, rules.weightMax);
}

} // namespace

Output AssignmentRules::build(random::Rng &rng, time::Window window,
                              std::uint32_t population) const {
  Output out;

  const auto count = operatorCountFor(*this, population, window.days);
  if (count == 0) {
    out.index();
    return out;
  }

  const auto windowStart = window.start;
  const auto windowStartEpoch = time::toEpochSeconds(windowStart);
  const auto windowDays = window.days;
  const auto windowEndEpoch =
      windowStartEpoch + static_cast<std::int64_t>(windowDays) * 86'400;

  out.operators.reserve(count);
  out.devices.reserve(static_cast<std::size_t>(count) * 3U);
  out.deviceTenures.reserve(static_cast<std::size_t>(count) * 3U);
  out.ips.reserve(static_cast<std::size_t>(count) * 8U);
  out.ipTenures.reserve(static_cast<std::size_t>(count) * 8U);

  for (std::uint32_t i = 0; i < count; ++i) {
    // THE LENGTH FIRST, then the start, then the weight, then the
    // inventory. Draw order is fixed and uniform across operators so the
    // stream does not depend on how many lines an earlier operator
    // happened to take. Length precedes start because the start's own
    // support depends on it.
    const auto lengthRaw =
        dist::lognormalByMedian(rng, campaignMedianDays, campaignSigma);
    const auto length = static_cast<std::int32_t>(std::clamp<double>(
        std::round(lengthRaw), static_cast<double>(campaignMinDays),
        static_cast<double>(campaignMaxDays)));

    // THE START MAY PRECEDE THE WINDOW, and it must.
    //
    // Drawing it over [0, W) would leave the first ~L days of every run
    // covered only by campaigns that happen to begin inside them, so
    // early-window compromises would systematically fail to attribute to
    // any operator and collapse into the victim-endpoint branch. That is
    // a HARNESS ARTIFACT masquerading as a mechanism — the corpus window
    // is an observation window, not the beginning of crime.
    //
    // Drawing over [-L, W) makes in-window coverage uniform: a campaign
    // already running at window start is caught mid-career, exactly as
    // one still running at window end is truncated at the other edge.
    // The gate's B' sub-gate measures the resulting concurrency and is
    // what surfaced the original one-sided version.
    const auto startOffset =
        static_cast<std::int64_t>(rng.choiceIndex(static_cast<std::size_t>(
            std::max<std::int64_t>(1, static_cast<std::int64_t>(windowDays) +
                                          length)))) -
        static_cast<std::int64_t>(length);

    const auto rawStartEpoch =
        time::toEpochSeconds(windowStart) + startOffset * 86'400;

    // CLIPPED TO THE WINDOW ON BOTH ENDS. Endpoint tenures are generated
    // over the clipped interval only: a tenure outside the generation
    // window is unreachable by any row, so generating one would cost
    // memory and buy nothing.
    const auto campaignStartEpoch = std::max(windowStartEpoch, rawStartEpoch);
    const auto campaignEndEpoch =
        std::min(windowEndEpoch,
                 rawStartEpoch + static_cast<std::int64_t>(length) * 86'400);

    const auto weight = sampleWeight(rng, *this);

    // A campaign clipped to nothing (it ended before the window opened,
    // which the [-L, W) support makes possible only at the boundary) is
    // still emitted with a one-day horizon rather than skipped: skipping
    // would make the operator COUNT depend on the draws, and the count is
    // what the sizing rule controls.
    const auto campaignStart = time::fromEpochSeconds(campaignStartEpoch);

    // Campaign days actually available inside the window. At least one:
    // sampleChain refuses a non-positive span, and an operator with no
    // endpoints would be selectable but unresolvable.
    const auto usableDays = static_cast<std::int32_t>(std::max<std::int64_t>(
        1, (campaignEndEpoch - campaignStartEpoch) / 86'400));

    const std::uint32_t deviceLines =
        1U + (rng.coin(secondDeviceLineP) ? 1U : 0U) +
        (rng.coin(thirdDeviceLineP) ? 1U : 0U);
    const std::uint32_t ipLines = 1U + (rng.coin(secondIpLineP) ? 1U : 0U) +
                                 (rng.coin(thirdIpLineP) ? 1U : 0U) +
                                 (rng.coin(fourthIpLineP) ? 1U : 0U);

    AttackerOperator op{};
    op.campaignFirstEpoch = campaignStartEpoch;
    op.campaignLastEpochExcl = std::max(campaignStartEpoch + 1, campaignEndEpoch);
    op.weight = weight;
    op.deviceBegin = static_cast<std::uint32_t>(out.devices.size());
    op.ipBegin = static_cast<std::uint32_t>(out.ips.size());

    // DEVICE LINES. Emitted CONTIGUOUSLY per line, exactly as the
    // customer generator does, and each line's chain TILES the campaign
    // — so at any instant inside the campaign the live device count
    // equals `deviceLines` and a point query can never come up empty.
    std::uint32_t slot = 0;
    for (std::uint32_t line = 0; line < deviceLines; ++line) {
      const auto chain = timeline::sampleChain(
          rng, campaignStart, usableDays,
          [](time::TimePoint) { return tenure::attackerDeviceTenureDays(); });

      for (const auto &link : chain) {
        out.devices.push_back(::PhantomLedger::devices::Identity{
            .ownerType = ::PhantomLedger::devices::OwnerType::ring,
            .ownerId = ::PhantomLedger::infra::kAttackerOwnerIdBase +
                       static_cast<std::uint64_t>(i),
            .slot = slot,
        });
        ++slot;
        out.deviceTenures.push_back(Tenure{
            .firstEpoch = time::toEpochSeconds(link.firstSeen),
            .lastEpochExcl = time::toEpochSeconds(link.lastSeenExcl),
        });
      }
    }

    for (std::uint32_t line = 0; line < ipLines; ++line) {
      const auto chain = timeline::sampleChain(
          rng, campaignStart, usableDays,
          [](time::TimePoint) { return tenure::attackerIpTenureDays(); });

      for (const auto &link : chain) {
        // THE SAME DISTRIBUTION LEGITIMATE ADDRESSES DRAW FROM
        // (shortcut #2, closed in card-fraud-realism-v2 step b and
        // preserved here). The intended signal is REUSE, never the
        // address value.
        out.ips.push_back(network::randomIpv4(rng));
        out.ipTenures.push_back(Tenure{
            .firstEpoch = time::toEpochSeconds(link.firstSeen),
            .lastEpochExcl = time::toEpochSeconds(link.lastSeenExcl),
        });
      }
    }

    op.deviceEnd = static_cast<std::uint32_t>(out.devices.size());
    op.ipEnd = static_cast<std::uint32_t>(out.ips.size());
    out.operators.push_back(op);
  }

  out.index();
  return out;
}

} // namespace PhantomLedger::synth::infra::attackers
