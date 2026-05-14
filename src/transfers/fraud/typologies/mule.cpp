#include "phantomledger/transfers/fraud/typologies/mule.hpp"

#include "phantomledger/math/amounts.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/fraud/schedule.hpp"
#include "phantomledger/transfers/fraud/typologies/common.hpp"

namespace PhantomLedger::transfers::fraud::typologies::mule {

std::vector<transactions::Transaction>
generate(IllicitContext &ctx, const Plan &plan, std::int32_t budget) {
  std::vector<transactions::Transaction> out;
  if (budget <= 0) {
    return out;
  }
  if (plan.muleAccounts.empty()) {
    return out;
  }
  if (plan.fraudAccounts.empty() && plan.victimAccounts.empty()) {
    return out;
  }

  random::Rng &rng = *ctx.execution.rng;

  const auto burst = sampleBurstWindow(rng, ctx.window.start, ctx.window.days,
                                       BurstShape{
                                           .tailPaddingDays = 14,
                                           .minDays = 3,
                                           .maxDays = 11,
                                       });

  const auto inChannel = channels::tag(channels::Fraud::muleIn);
  const auto forwardChannel = channels::tag(channels::Fraud::muleForward);

  for (const auto &mule : plan.muleAccounts) {
    if (static_cast<std::int32_t>(out.size()) >= budget) {
      break;
    }

    const auto inboundCount = static_cast<std::int32_t>(rng.uniformInt(8, 26));

    std::vector<entity::Key> sourcePool;
    sourcePool.reserve(plan.victimAccounts.size() + 10);
    sourcePool.insert(sourcePool.end(), plan.victimAccounts.begin(),
                      plan.victimAccounts.end());

    if (!ctx.billerAccounts.empty()) {
      const auto extraCap = static_cast<std::size_t>(rng.uniformInt(3, 10));
      const auto nExtra = std::min(extraCap, ctx.billerAccounts.size());
      const auto extraIdx = rng.choiceIndices(ctx.billerAccounts.size(), nExtra,
                                              /*replace=*/false);
      for (const auto i : extraIdx) {
        sourcePool.push_back(ctx.billerAccounts[i]);
      }
    }

    if (sourcePool.empty()) {
      continue;
    }

    // Forward targets: organizers if any, else other mules.
    std::vector<entity::Key> forwardTargets;
    if (!plan.fraudAccounts.empty()) {
      forwardTargets = plan.fraudAccounts;
    } else {
      forwardTargets.reserve(plan.muleAccounts.size());
      for (const auto &m : plan.muleAccounts) {
        if (m != mule) {
          forwardTargets.push_back(m);
        }
      }
      if (forwardTargets.empty()) {
        continue;
      }
    }

    for (std::int32_t inboundIdx = 0; inboundIdx < inboundCount; ++inboundIdx) {
      if (static_cast<std::int32_t>(out.size()) >= budget) {
        break;
      }

      const auto &src =
          sourcePool[static_cast<std::size_t>(inboundIdx) % sourcePool.size()];
      if (src == mule) {
        continue;
      }

      // Inbound amount: mostly fraud-scale, 40% chance to swap in 3× P2P.
      double inboundAmt = math::amounts::kFraud.sample(rng);
      if (rng.coin(0.40)) {
        inboundAmt = math::amounts::kP2P.sample(rng) * 3.0;
      }
      inboundAmt = primitives::utils::floorAndRound(inboundAmt, 50.0);

      const auto inboundTs =
          sampleTimestamp(rng, burst.baseDate, burst.durationDays,
                          HourRange{.min = 7, .max = 22});

      if (!typologies::appendBoundedTxn(
              ctx, out, budget,
              transactions::Draft{
                  .source = src,
                  .destination = mule,
                  .amount = inboundAmt,
                  .timestamp = time::toEpochSeconds(inboundTs),
                  .isFraud = 1,
                  .ringId = static_cast<std::int32_t>(plan.ringId),
                  .channel = inChannel,
              })) {
        break;
      }

      if (static_cast<std::int32_t>(out.size()) >= budget) {
        break;
      }

      const double haircut = 0.05 + 0.05 * rng.nextDouble();
      const double forwardAmt =
          primitives::utils::roundMoney(inboundAmt * (1.0 - haircut));
      if (forwardAmt < 10.0) {
        continue;
      }

      const auto delayHours = static_cast<std::int32_t>(rng.uniformInt(1, 14));
      const auto delayMinutes =
          static_cast<std::int32_t>(rng.uniformInt(0, 61));
      const auto forwardTs =
          inboundTs + time::Hours{delayHours} + time::Minutes{delayMinutes};

      const auto &forwardDst =
          forwardTargets[rng.choiceIndex(forwardTargets.size())];

      if (!typologies::appendBoundedTxn(
              ctx, out, budget,
              transactions::Draft{
                  .source = mule,
                  .destination = forwardDst,
                  .amount = forwardAmt,
                  .timestamp = time::toEpochSeconds(forwardTs),
                  .isFraud = 1,
                  .ringId = static_cast<std::int32_t>(plan.ringId),
                  .channel = forwardChannel,
              })) {
        break;
      }
    }
  }

  return out;
}

} // namespace PhantomLedger::transfers::fraud::typologies::mule
