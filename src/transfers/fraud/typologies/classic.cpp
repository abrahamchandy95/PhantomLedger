#include "phantomledger/transfers/fraud/typologies/classic.hpp"

#include "phantomledger/math/amounts.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/fraud/schedule.hpp"
#include "phantomledger/transfers/fraud/typologies/common.hpp"

#include <algorithm>

namespace PhantomLedger::transfers::fraud::typologies::classic {

namespace {

class DraftMaker {
public:
  DraftMaker(std::uint32_t ringId, channels::Tag channel,
             std::int32_t chainId) noexcept
      : ringId_(ringId), channel_(channel), chainId_(chainId) {}

  [[nodiscard]] transactions::Draft make(const entity::Key &source,
                                         const entity::Key &destination,
                                         double amount,
                                         time::TimePoint ts) const {
    return transactions::Draft{
        .source = source,
        .destination = destination,
        .amount = amount,
        .timestamp = time::toEpochSeconds(ts),
        .isFraud = 1,
        .ringId = static_cast<std::int32_t>(ringId_),
        .channel = channel_,
        .chainId = chainId_,
    };
  }

private:
  std::uint32_t ringId_;
  channels::Tag channel_;
  std::int32_t chainId_;
};

} // namespace

std::vector<transactions::Transaction>
generate(IllicitContext &ctx, const Plan &plan, std::int32_t budget) {
  std::vector<transactions::Transaction> out;
  if (budget <= 0) {
    return out;
  }

  random::Rng &rng = *ctx.execution.rng;

  const auto burst = sampleBurstWindow(rng, ctx.window.start, ctx.window.days,
                                       BurstShape{
                                           .tailPaddingDays = 7,
                                           .minDays = 2,
                                           .maxDays = 6,
                                       });

  const auto fraudChannel = channels::tag(channels::Fraud::classic);
  const auto cycleChannel = channels::tag(channels::Fraud::cycle);

  // Two chain ids for classic: one groups the splash (phase 1+2 = victim →
  // mule → fraud); one groups the cycle (phase 3 = ring nodes passing money
  // among themselves). These are structurally distinct operations.
  const auto splashChainId = ctx.allocateChainId();
  const auto cycleChainId = ctx.allocateChainId();

  const DraftMaker fraudDraft{plan.ringId, fraudChannel, splashChainId};
  const DraftMaker cycleDraft{plan.ringId, cycleChannel, cycleChainId};

  // ---- Phase 1: victim → mule -----------------------------------------
  for (const auto &victim : plan.victimAccounts) {
    if (static_cast<std::int32_t>(out.size()) >= budget) {
      break;
    }
    if (plan.muleAccounts.empty() || !rng.coin(0.75)) {
      continue;
    }

    const auto &mule = typologies::pickOne(rng, plan.muleAccounts);
    const auto ts = sampleTimestamp(rng, burst.baseDate, burst.durationDays,
                                    HourRange{.min = 8, .max = 22});
    if (!typologies::appendBoundedTxn(
            ctx, out, budget,
            fraudDraft.make(victim, mule, math::amounts::kFraud.sample(rng),
                            ts))) {
      break;
    }
  }

  // ---- Phase 2: mule → fraud ------------------------------------------
  for (const auto &mule : plan.muleAccounts) {
    if (static_cast<std::int32_t>(out.size()) >= budget) {
      break;
    }
    if (plan.fraudAccounts.empty()) {
      continue;
    }

    const auto forwards = static_cast<std::int32_t>(rng.uniformInt(2, 7));
    const auto spanDays = std::min<std::int32_t>(3, burst.durationDays);

    for (std::int32_t i = 0; i < forwards; ++i) {
      if (static_cast<std::int32_t>(out.size()) >= budget) {
        break;
      }

      const auto &fraudAcct = typologies::pickOne(rng, plan.fraudAccounts);
      const auto ts = sampleTimestamp(rng, burst.baseDate, spanDays,
                                      HourRange{.min = 0, .max = 24});
      if (!typologies::appendBoundedTxn(
              ctx, out, budget,
              fraudDraft.make(mule, fraudAcct,
                              math::amounts::kFraud.sample(rng), ts))) {
        break;
      }
    }
  }

  // ---- Phase 3: cycle pass through ring nodes -------------------------
  const auto nodes = plan.participantAccounts();
  if (nodes.size() < 3 || static_cast<std::int32_t>(out.size()) >= budget) {
    return out;
  }

  const auto cycleTarget = static_cast<std::size_t>(rng.uniformInt(3, 8));
  const auto cycleSize = std::min(nodes.size(), cycleTarget);
  const auto cycleNodes = typologies::pickK(rng, nodes, cycleSize);
  if (cycleNodes.size() < 2) {
    return out;
  }

  const auto passes = static_cast<std::int32_t>(rng.uniformInt(2, 6));

  for (std::int32_t pass = 0; pass < passes; ++pass) {
    if (static_cast<std::int32_t>(out.size()) >= budget) {
      break;
    }

    for (std::size_t idx = 0; idx < cycleNodes.size(); ++idx) {
      if (static_cast<std::int32_t>(out.size()) >= budget) {
        break;
      }
      const auto &src = cycleNodes[idx];
      const auto &dst = cycleNodes[(idx + 1) % cycleNodes.size()];

      const auto ts = sampleTimestamp(rng, burst.baseDate, burst.durationDays,
                                      HourRange{.min = 0, .max = 24});
      if (!typologies::appendBoundedTxn(
              ctx, out, budget,
              cycleDraft.make(src, dst, math::amounts::kFraudCycle.sample(rng),
                              ts))) {
        break;
      }
    }
  }

  return out;
}

} // namespace PhantomLedger::transfers::fraud::typologies::classic
