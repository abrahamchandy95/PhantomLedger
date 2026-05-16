#include "phantomledger/transfers/fraud/typologies/classic.hpp"

#include "phantomledger/math/amounts.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/fraud/schedule.hpp"
#include "phantomledger/transfers/fraud/typologies/common.hpp"

#include <algorithm>
#include <cstdint>

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

void runVictimToMule(IllicitContext &ctx,
                     std::vector<transactions::Transaction> &out,
                     std::int32_t budget, const Plan &plan,
                     const BurstWindow &burst, const DraftMaker &maker) {
  random::Rng &rng = *ctx.execution.rng;
  for (const auto &victim : plan.victimAccounts) {
    if (static_cast<std::int32_t>(out.size()) >= budget) {
      return;
    }
    if (plan.muleAccounts.empty() || !rng.coin(0.75)) {
      continue;
    }
    const auto &mule = typologies::pickOne(rng, plan.muleAccounts);
    const auto ts = sampleTimestamp(rng, burst.baseDate, burst.durationDays,
                                    HourRange{.min = 8, .max = 22});
    if (!typologies::appendBoundedTxn(
            ctx, out, budget,
            maker.make(victim, mule, math::amounts::kFraud.sample(rng), ts))) {
      return;
    }
  }
}

void runMuleToFraud(IllicitContext &ctx,
                    std::vector<transactions::Transaction> &out,
                    std::int32_t budget, const Plan &plan,
                    const BurstWindow &burst, const DraftMaker &maker) {
  random::Rng &rng = *ctx.execution.rng;
  for (const auto &mule : plan.muleAccounts) {
    if (static_cast<std::int32_t>(out.size()) >= budget) {
      return;
    }
    if (plan.fraudAccounts.empty()) {
      continue;
    }
    const auto forwards = static_cast<std::int32_t>(rng.uniformInt(2, 7));
    const auto spanDays = std::min<std::int32_t>(3, burst.durationDays);
    for (std::int32_t i = 0; i < forwards; ++i) {
      if (static_cast<std::int32_t>(out.size()) >= budget) {
        return;
      }
      const auto &fraudAcct = typologies::pickOne(rng, plan.fraudAccounts);
      const auto ts = sampleTimestamp(rng, burst.baseDate, spanDays,
                                      HourRange{.min = 0, .max = 24});
      if (!typologies::appendBoundedTxn(
              ctx, out, budget,
              maker.make(mule, fraudAcct, math::amounts::kFraud.sample(rng),
                         ts))) {
        return;
      }
    }
  }
}

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

  const auto chainId = ctx.allocateChainId();
  const DraftMaker maker{plan.ringId, channels::tag(channels::Fraud::classic),
                         chainId};

  runVictimToMule(ctx, out, budget, plan, burst, maker);
  runMuleToFraud(ctx, out, budget, plan, burst, maker);

  return out;
}

} // namespace PhantomLedger::transfers::fraud::typologies::classic
