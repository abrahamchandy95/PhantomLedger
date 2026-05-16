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

constexpr double kShellBiasProbability = 0.65;

struct Roles {
  entity::Key origin;
  std::vector<entity::Key> intermediaries;
  entity::Key beneficiary;
};

struct Emit {
  IllicitContext &ctx;
  std::vector<transactions::Transaction> &out;
  std::int32_t budget;
};

struct LegFrame {
  std::uint32_t ringId;
  std::int32_t chainId;
  channels::Tag channel;
  time::TimePoint baseTs;
};

struct EventFrame {
  const entity::Key &origin;
  const BurstWindow &burst;
  channels::Tag splitChannel;
  channels::Tag mergeChannel;
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

[[nodiscard]] bool inVector(const std::vector<entity::Key> &v,
                            const entity::Key &k) noexcept {
  return std::find(v.begin(), v.end(), k) != v.end();
}

void swapIntermediariesForShells(std::vector<entity::Key> &intermediaries,
                                 const std::vector<entity::Key> &shells,
                                 random::Rng &rng) {
  if (shells.empty()) {
    return;
  }
  for (auto &slot : intermediaries) {
    if (!rng.coin(kShellBiasProbability)) {
      continue;
    }
    const auto &candidate = shells[rng.choiceIndex(shells.size())];
    if (inVector(intermediaries, candidate)) {
      continue;
    }
    slot = candidate;
  }
}

bool emitSplitLeg(Emit e, LegFrame frame, const Roles &roles,
                  std::span<const double> splitAmounts) {
  random::Rng &rng = *e.ctx.execution.rng;
  for (std::size_t i = 0; i < roles.intermediaries.size(); ++i) {
    if (static_cast<std::int32_t>(e.out.size()) >= e.budget) {
      return false;
    }
    const auto delay =
        rng.uniformInt(kSplitDelayMinutesLo, kSplitDelayMinutesHi + 1);
    const auto ts =
        frame.baseTs + time::Minutes{delay * static_cast<std::int64_t>(i)};

    if (!typologies::appendBoundedTxn(
            e.ctx, e.out, e.budget,
            transactions::Draft{
                .source = roles.origin,
                .destination = roles.intermediaries[i],
                .amount = primitives::utils::roundMoney(splitAmounts[i]),
                .timestamp = time::toEpochSeconds(ts),
                .isFraud = 1,
                .ringId = static_cast<std::int32_t>(frame.ringId),
                .channel = frame.channel,
                .chainId = frame.chainId,
            })) {
      return false;
    }
  }
  return true;
}

void emitMergeLeg(Emit e, LegFrame frame, const Roles &roles,
                  std::span<const double> splitAmounts) {
  random::Rng &rng = *e.ctx.execution.rng;
  for (std::size_t i = 0; i < roles.intermediaries.size(); ++i) {
    if (static_cast<std::int32_t>(e.out.size()) >= e.budget) {
      return;
    }
    const double amt =
        primitives::utils::roundMoney(mergeAmount(splitAmounts[i], rng));
    if (amt < kAmountFloor) {
      continue;
    }
    const auto delay =
        rng.uniformInt(kMergeDelayMinutesLo, kMergeDelayMinutesHi + 1);
    const auto ts = frame.baseTs + time::Minutes{delay};

    if (!typologies::appendBoundedTxn(
            e.ctx, e.out, e.budget,
            transactions::Draft{
                .source = roles.intermediaries[i],
                .destination = roles.beneficiary,
                .amount = amt,
                .timestamp = time::toEpochSeconds(ts),
                .isFraud = 1,
                .ringId = static_cast<std::int32_t>(frame.ringId),
                .channel = frame.channel,
                .chainId = frame.chainId,
            })) {
      return;
    }
  }
}

[[nodiscard]] Roles buildRoles(random::Rng &rng, const Plan &plan,
                               const entity::Key &origin, std::size_t n) {
  std::vector<entity::Key> intermediaries =
      typologies::pickK(rng, plan.muleAccounts, n);
  swapIntermediariesForShells(intermediaries, plan.shellFraudAccounts, rng);
  return Roles{
      .origin = origin,
      .intermediaries = std::move(intermediaries),
      .beneficiary =
          plan.fraudAccounts[rng.choiceIndex(plan.fraudAccounts.size())],
  };
}

[[nodiscard]] std::vector<double>
buildSplitAmounts(random::Rng &rng, double principal, std::size_t n) {
  const double evenSplit = principal / static_cast<double>(n);
  std::vector<double> amounts(n);
  for (std::size_t i = 0; i < n; ++i) {
    amounts[i] = jitteredSplit(evenSplit, rng);
  }
  return amounts;
}

bool runOneEvent(Emit e, const Plan &plan, const EventFrame &frame) {
  random::Rng &rng = *e.ctx.execution.rng;

  const auto n = pickIntermediaryCount(rng, plan.muleAccounts.size());
  if (n < 2) {
    return true;
  }

  const auto roles = buildRoles(rng, plan, frame.origin, n);
  const auto principal = samplePrincipal(rng, n);
  const auto splitAmounts = buildSplitAmounts(rng, principal, n);

  const auto chainId = e.ctx.allocateChainId();
  const auto baseTs =
      sampleTimestamp(rng, frame.burst.baseDate, frame.burst.durationDays,
                      HourRange{.min = 8, .max = 22});

  const LegFrame splitFrame{plan.ringId, chainId, frame.splitChannel, baseTs};
  if (!emitSplitLeg(e, splitFrame, roles,
                    std::span<const double>(splitAmounts))) {
    return false;
  }

  const auto afterSplitTs =
      baseTs +
      time::Minutes{kSplitDelayMinutesHi * static_cast<std::int64_t>(n + 1)};
  const LegFrame mergeFrame{plan.ringId, chainId, frame.mergeChannel,
                            afterSplitTs};
  emitMergeLeg(e, mergeFrame, roles, std::span<const double>(splitAmounts));

  return static_cast<std::int32_t>(e.out.size()) < e.budget;
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
    const Emit emit{ctx, out, budget};
    const EventFrame frame{origin, burst, splitChannel, mergeChannel};
    if (!runOneEvent(emit, plan, frame)) {
      break;
    }
  }

  return out;
}

} // namespace PhantomLedger::transfers::fraud::typologies::scatterGather
