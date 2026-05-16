#include "phantomledger/transfers/fraud/typologies/scatter_gather.hpp"

#include "phantomledger/math/amounts.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/fraud/schedule.hpp"
#include "phantomledger/transfers/fraud/typologies/classic.hpp"
#include "phantomledger/transfers/fraud/typologies/common.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace PhantomLedger::transfers::fraud::typologies::scatterGather {

namespace {

constexpr std::int64_t kIntermediariesLo = 3;
constexpr std::int64_t kIntermediariesHi = 8;

constexpr std::int64_t kSplitDelayMinutesLo = 1;
constexpr std::int64_t kSplitDelayMinutesHi = 15;

constexpr std::int64_t kMergeDelayMinutesLo = 30;
constexpr std::int64_t kMergeDelayMinutesHi = 480;

constexpr double kSplitJitterFraction = 0.15;
constexpr double kMergeHaircutMin = 0.03;
constexpr double kMergeHaircutRange = 0.05;

constexpr double kAmountFloor = 50.0;
constexpr double kVictimEntryProb = 0.55;

struct Roles {
  entity::Key origin;
  std::vector<entity::Key> intermediaries;
  entity::Key beneficiary;
};

[[nodiscard]] bool hasViableShape(const Plan &plan) noexcept {
  return !plan.victimAccounts.empty() && plan.muleAccounts.size() >= 2 &&
         !plan.fraudAccounts.empty();
}

[[nodiscard]] std::size_t pickIntermediaryCount(random::Rng &rng,
                                                std::size_t maxAvailable) {
  const auto req = static_cast<std::size_t>(
      rng.uniformInt(kIntermediariesLo, kIntermediariesHi + 1));
  return std::min(req, maxAvailable);
}

[[nodiscard]] double samplePrincipal(random::Rng &rng,
                                     std::size_t intermediaryCount) {
  const double minPrincipal =
      kAmountFloor * static_cast<double>(intermediaryCount) * 2.0;
  double p = math::amounts::kFraud.sample(rng);
  if (p < minPrincipal) {
    p = minPrincipal;
  }
  return p;
}

[[nodiscard]] double jitteredSplit(double evenSplit,
                                   random::Rng &rng) noexcept {
  const double jitter = (rng.nextDouble() - 0.5) * 2.0 * kSplitJitterFraction;
  return std::max(kAmountFloor, evenSplit * (1.0 + jitter));
}

[[nodiscard]] double mergeAmount(double splitAmt, random::Rng &rng) noexcept {
  const double cut = kMergeHaircutMin + kMergeHaircutRange * rng.nextDouble();
  return splitAmt * (1.0 - cut);
}

bool emitSplitLeg(IllicitContext &ctx,
                  std::vector<transactions::Transaction> &out,
                  std::int32_t budget, const Roles &roles,
                  std::span<const double> splitAmounts, time::TimePoint baseTs,
                  channels::Tag channel, std::uint32_t ringId,
                  std::int32_t chainId) {
  random::Rng &rng = *ctx.execution.rng;
  for (std::size_t i = 0; i < roles.intermediaries.size(); ++i) {
    if (static_cast<std::int32_t>(out.size()) >= budget) {
      return false;
    }
    const auto delay =
        rng.uniformInt(kSplitDelayMinutesLo, kSplitDelayMinutesHi + 1);
    const auto ts =
        baseTs + time::Minutes{delay * static_cast<std::int64_t>(i)};

    if (!typologies::appendBoundedTxn(
            ctx, out, budget,
            transactions::Draft{
                .source = roles.origin,
                .destination = roles.intermediaries[i],
                .amount = primitives::utils::roundMoney(splitAmounts[i]),
                .timestamp = time::toEpochSeconds(ts),
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

void emitMergeLeg(IllicitContext &ctx,
                  std::vector<transactions::Transaction> &out,
                  std::int32_t budget, const Roles &roles,
                  std::span<const double> splitAmounts,
                  time::TimePoint afterSplitTs, channels::Tag channel,
                  std::uint32_t ringId, std::int32_t chainId) {
  random::Rng &rng = *ctx.execution.rng;
  for (std::size_t i = 0; i < roles.intermediaries.size(); ++i) {
    if (static_cast<std::int32_t>(out.size()) >= budget) {
      return;
    }
    const double amt =
        primitives::utils::roundMoney(mergeAmount(splitAmounts[i], rng));
    if (amt < kAmountFloor) {
      continue;
    }
    const auto delay =
        rng.uniformInt(kMergeDelayMinutesLo, kMergeDelayMinutesHi + 1);
    const auto ts = afterSplitTs + time::Minutes{delay};

    if (!typologies::appendBoundedTxn(
            ctx, out, budget,
            transactions::Draft{
                .source = roles.intermediaries[i],
                .destination = roles.beneficiary,
                .amount = amt,
                .timestamp = time::toEpochSeconds(ts),
                .isFraud = 1,
                .ringId = static_cast<std::int32_t>(ringId),
                .channel = channel,
                .chainId = chainId,
            })) {
      return;
    }
  }
}

bool runOneEvent(IllicitContext &ctx,
                 std::vector<transactions::Transaction> &out,
                 std::int32_t budget, const Plan &plan,
                 const entity::Key &origin, const BurstWindow &burst,
                 channels::Tag splitChannel, channels::Tag mergeChannel) {
  random::Rng &rng = *ctx.execution.rng;

  const auto n = pickIntermediaryCount(rng, plan.muleAccounts.size());
  if (n < 2) {
    return true;
  }

  const Roles roles{
      .origin = origin,
      .intermediaries = typologies::pickK(rng, plan.muleAccounts, n),
      .beneficiary =
          plan.fraudAccounts[rng.choiceIndex(plan.fraudAccounts.size())],
  };

  const double principal = samplePrincipal(rng, n);
  const double evenSplit = principal / static_cast<double>(n);

  std::vector<double> splitAmounts(n);
  for (std::size_t i = 0; i < n; ++i) {
    splitAmounts[i] = jitteredSplit(evenSplit, rng);
  }

  const auto chainId = ctx.allocateChainId();
  const auto baseTs = sampleTimestamp(rng, burst.baseDate, burst.durationDays,
                                      HourRange{.min = 8, .max = 22});

  if (!emitSplitLeg(ctx, out, budget, roles, splitAmounts, baseTs, splitChannel,
                    plan.ringId, chainId)) {
    return false;
  }

  const auto afterSplitTs =
      baseTs +
      time::Minutes{kSplitDelayMinutesHi * static_cast<std::int64_t>(n + 1)};
  emitMergeLeg(ctx, out, budget, roles, splitAmounts, afterSplitTs,
               mergeChannel, plan.ringId, chainId);

  return static_cast<std::int32_t>(out.size()) < budget;
}

} // namespace

std::vector<transactions::Transaction>
generate(IllicitContext &ctx, const Plan &plan, std::int32_t budget) {
  std::vector<transactions::Transaction> out;
  if (budget <= 0) {
    return out;
  }
  if (!hasViableShape(plan)) {
    return classic::generate(ctx, plan, budget);
  }

  random::Rng &rng = *ctx.execution.rng;

  const auto burst = sampleBurstWindow(rng, ctx.window.start, ctx.window.days,
                                       BurstShape{
                                           .tailPaddingDays = 7,
                                           .minDays = 2,
                                           .maxDays = 5,
                                       });

  const auto splitChannel = channels::tag(channels::Fraud::scatterGatherSplit);
  const auto mergeChannel = channels::tag(channels::Fraud::scatterGatherMerge);

  for (const auto &origin : plan.victimAccounts) {
    if (static_cast<std::int32_t>(out.size()) >= budget) {
      break;
    }
    if (!rng.coin(kVictimEntryProb)) {
      continue;
    }
    if (!runOneEvent(ctx, out, budget, plan, origin, burst, splitChannel,
                     mergeChannel)) {
      break;
    }
  }

  return out;
}

} // namespace PhantomLedger::transfers::fraud::typologies::scatterGather
