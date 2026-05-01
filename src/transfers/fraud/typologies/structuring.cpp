#include "phantomledger/transfers/fraud/typologies/structuring.hpp"

#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/fraud/schedule.hpp"
#include "phantomledger/transfers/fraud/typologies/common.hpp"

#include <algorithm>

namespace PhantomLedger::transfers::fraud::typologies::structuring {

std::vector<transactions::Transaction>
generate(IllicitContext &ctx, const Plan &plan, std::int32_t budget) {
  std::vector<transactions::Transaction> out;
  if (budget <= 0) {
    return out;
  }
  if (plan.muleAccounts.empty() && plan.fraudAccounts.empty()) {
    return out;
  }

  random::Rng &rng = *ctx.execution.rng;

  const auto burst =
      sampleBurstWindow(rng, ctx.window.startDate, ctx.window.days,
                        /*tailPaddingDays=*/10,
                        /*minBurstDays=*/3,
                        /*maxBurstDays=*/8);

  // Target: mule if available, otherwise fraud.
  const auto &target = !plan.muleAccounts.empty()
                           ? typologies::pickOne(rng, plan.muleAccounts)
                           : typologies::pickOne(rng, plan.fraudAccounts);

  const auto &rules = ctx.structuringRules;
  const double threshold = rules.threshold;
  const double epsMin = rules.epsilonMin;
  const double epsMax = rules.epsilonMax;
  const double epsRange = epsMax - epsMin;

  const auto channel = channels::tag(channels::Fraud::structuring);

  for (const auto &victim : plan.victimAccounts) {
    if (static_cast<std::int32_t>(out.size()) >= budget) {
      break;
    }
    if (!rng.coin(0.55)) {
      continue;
    }

    const auto splits = static_cast<std::int32_t>(rng.uniformInt(
        rules.splitsMin, static_cast<std::int64_t>(rules.splitsMax) + 1));

    for (std::int32_t i = 0; i < splits; ++i) {
      if (static_cast<std::int32_t>(out.size()) >= budget) {
        break;
      }

      const double eps = epsMin + epsRange * rng.nextDouble();
      const double amount = std::max(50.0, threshold - eps);
      const auto ts = sampleTimestamp(rng, burst.baseDate, burst.durationDays,
                                      /*minHour=*/8, /*maxHour=*/22);

      if (!typologies::appendBoundedTxn(
              ctx, out, budget,
              transactions::Draft{
                  .source = victim,
                  .destination = target,
                  .amount = amount,
                  .timestamp = time::toEpochSeconds(ts),
                  .isFraud = 1,
                  .ringId = static_cast<std::int32_t>(plan.ringId),
                  .channel = channel,
              })) {
        break;
      }
    }
  }

  return out;
}

} // namespace PhantomLedger::transfers::fraud::typologies::structuring
