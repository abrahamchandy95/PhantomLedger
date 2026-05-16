#include "phantomledger/transfers/fraud/typologies/cycle.hpp"

#include "phantomledger/math/amounts.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/fraud/schedule.hpp"
#include "phantomledger/transfers/fraud/typologies/classic.hpp"
#include "phantomledger/transfers/fraud/typologies/common.hpp"

#include <algorithm>
#include <cstdint>

namespace PhantomLedger::transfers::fraud::typologies::cycle {

namespace {

constexpr std::size_t kMinCycleSize = 3;
constexpr std::size_t kMaxCycleSize = 7;

constexpr std::int64_t kMinPasses = 2;
constexpr std::int64_t kMaxPasses = 5;

constexpr double kHopHaircutMin = 0.01;
constexpr double kHopHaircutRange = 0.02;

constexpr std::int64_t kIntraPassDelayMinutesLo = 5;
constexpr std::int64_t kIntraPassDelayMinutesHi = 180;

constexpr std::int64_t kInterPassDelayMinutesLo = 60;
constexpr std::int64_t kInterPassDelayMinutesHi = 720;

constexpr double kAmountFloor = 5.0;

[[nodiscard]] std::vector<entity::Key>
pickCycleNodes(random::Rng &rng, const std::vector<entity::Key> &pool) {
  const auto target = static_cast<std::size_t>(
      rng.uniformInt(static_cast<std::int64_t>(kMinCycleSize),
                     static_cast<std::int64_t>(kMaxCycleSize) + 1));
  return typologies::pickK(rng, pool, std::min(pool.size(), target));
}

[[nodiscard]] double samplePrincipal(random::Rng &rng) noexcept {
  double p = math::amounts::kFraudCycle.sample(rng);
  if (p < kAmountFloor * 2.0) {
    p = kAmountFloor * 2.0;
  }
  return p;
}

[[nodiscard]] double applyHaircut(double principal, random::Rng &rng) noexcept {
  const double cut = kHopHaircutMin + kHopHaircutRange * rng.nextDouble();
  return principal * (1.0 - cut);
}

[[nodiscard]] time::TimePoint advanceTs(time::TimePoint ts, random::Rng &rng,
                                        std::int64_t lo,
                                        std::int64_t hi) noexcept {
  return ts + time::Minutes{rng.uniformInt(lo, hi + 1)};
}

bool runOnePass(IllicitContext &ctx,
                std::vector<transactions::Transaction> &out,
                std::int32_t budget, const std::vector<entity::Key> &nodes,
                channels::Tag channel, std::uint32_t ringId,
                std::int32_t chainId, double principal,
                time::TimePoint &currentTs) {
  random::Rng &rng = *ctx.execution.rng;
  for (std::size_t idx = 0; idx < nodes.size(); ++idx) {
    if (static_cast<std::int32_t>(out.size()) >= budget) {
      return false;
    }
    const auto &src = nodes[idx];
    const auto &dst = nodes[(idx + 1) % nodes.size()];

    principal = applyHaircut(principal, rng);
    if (principal < kAmountFloor) {
      return true;
    }

    currentTs = advanceTs(currentTs, rng, kIntraPassDelayMinutesLo,
                          kIntraPassDelayMinutesHi);

    if (!typologies::appendBoundedTxn(
            ctx, out, budget,
            transactions::Draft{
                .source = src,
                .destination = dst,
                .amount = primitives::utils::roundMoney(principal),
                .timestamp = time::toEpochSeconds(currentTs),
                .isFraud = 1,
                .ringId = static_cast<std::int32_t>(ringId),
                .channel = channel,
                .chainId = chainId,
            })) {
      return false;
    }
  }
  return true;
}

} // namespace

std::vector<transactions::Transaction>
generate(IllicitContext &ctx, const Plan &plan, std::int32_t budget) {
  std::vector<transactions::Transaction> out;
  if (budget <= 0) {
    return out;
  }

  const auto participants = plan.participantAccounts();
  if (participants.size() < kMinCycleSize) {
    return classic::generate(ctx, plan, budget);
  }

  random::Rng &rng = *ctx.execution.rng;

  const auto burst = sampleBurstWindow(rng, ctx.window.start, ctx.window.days,
                                       BurstShape{
                                           .tailPaddingDays = 7,
                                           .minDays = 2,
                                           .maxDays = 6,
                                       });

  const auto cycleNodes = pickCycleNodes(rng, participants);
  if (cycleNodes.size() < kMinCycleSize) {
    return out;
  }

  const auto passes =
      static_cast<std::int32_t>(rng.uniformInt(kMinPasses, kMaxPasses + 1));
  const auto channel = channels::tag(channels::Fraud::cycle);
  const auto chainId = ctx.allocateChainId();

  auto currentTs = sampleTimestamp(rng, burst.baseDate, burst.durationDays,
                                   HourRange{.min = 0, .max = 24});

  for (std::int32_t pass = 0; pass < passes; ++pass) {
    if (static_cast<std::int32_t>(out.size()) >= budget) {
      break;
    }
    const double principal = samplePrincipal(rng);
    if (!runOnePass(ctx, out, budget, cycleNodes, channel, plan.ringId, chainId,
                    principal, currentTs)) {
      break;
    }
    currentTs = advanceTs(currentTs, rng, kInterPassDelayMinutesLo,
                          kInterPassDelayMinutesHi);
  }

  return out;
}

} // namespace PhantomLedger::transfers::fraud::typologies::cycle
