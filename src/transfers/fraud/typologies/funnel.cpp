#include "phantomledger/transfers/fraud/typologies/funnel.hpp"

#include "phantomledger/math/amounts.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/fraud/schedule.hpp"
#include "phantomledger/transfers/fraud/typologies/classic.hpp"
#include "phantomledger/transfers/fraud/typologies/common.hpp"

#include <algorithm>

namespace PhantomLedger::transfers::fraud::typologies::funnel {

std::vector<transactions::Transaction>
generate(IllicitContext &ctx, const Plan &plan, std::int32_t budget) {
  std::vector<transactions::Transaction> out;
  if (budget <= 0) {
    return out;
  }

  random::Rng &rng = *ctx.execution.rng;

  const auto burst =
      sampleBurstWindow(rng, ctx.window.startDate, ctx.window.days,
                        /*tailPaddingDays=*/10,
                        /*minBurstDays=*/2,
                        /*maxBurstDays=*/6);

  // Pool: organizers + mules. Need >= 2 to have a distinct collector and
  // at least one separable cash-out path; otherwise fall back to classic.
  const auto pool = plan.participantAccounts();
  if (pool.size() < 2) {
    return classic::generate(ctx, plan, budget);
  }

  const auto &collector = pool[rng.choiceIndex(pool.size())];

  // Cash-out targets: up to 4 distinct nodes from pool, with collector
  // removed if present and there's at least one other candidate.
  auto cashouts =
      typologies::pickK(rng, pool, std::min<std::size_t>(4, pool.size()));
  if (cashouts.size() > 1) {
    cashouts.erase(std::remove(cashouts.begin(), cashouts.end(), collector),
                   cashouts.end());
  }

  const auto inChannel = channels::tag(channels::Fraud::funnelIn);
  const auto outChannel = channels::tag(channels::Fraud::funnelOut);

  // ---- Inbound: victims + mules → collector --------------------------
  std::vector<entity::Key> sources;
  sources.reserve(plan.victimAccounts.size() + plan.muleAccounts.size());
  sources.insert(sources.end(), plan.victimAccounts.begin(),
                 plan.victimAccounts.end());
  sources.insert(sources.end(), plan.muleAccounts.begin(),
                 plan.muleAccounts.end());

  for (const auto &src : sources) {
    if (static_cast<std::int32_t>(out.size()) >= budget) {
      break;
    }
    if (!rng.coin(0.55)) {
      continue;
    }

    const auto ts = sampleTimestamp(rng, burst.baseDate, burst.durationDays,
                                    /*minHour=*/8, /*maxHour=*/22);
    if (!typologies::appendBoundedTxn(
            ctx, out, budget,
            transactions::Draft{
                .source = src,
                .destination = collector,
                .amount = math::amounts::kFraud.sample(rng),
                .timestamp = time::toEpochSeconds(ts),
                .isFraud = 1,
                .ringId = static_cast<std::int32_t>(plan.ringId),
                .channel = inChannel,
            })) {
      break;
    }
  }

  // ---- Outbound: collector → cash-out targets ------------------------
  const auto outboundCount = static_cast<std::int32_t>(rng.uniformInt(6, 17));
  for (std::int32_t i = 0; i < outboundCount; ++i) {
    if (static_cast<std::int32_t>(out.size()) >= budget) {
      break;
    }

    const auto &dst = !cashouts.empty()
                          ? cashouts[rng.choiceIndex(cashouts.size())]
                          : collector;

    const auto ts = sampleTimestamp(rng, burst.baseDate, burst.durationDays,
                                    /*minHour=*/0, /*maxHour=*/24);
    if (!typologies::appendBoundedTxn(
            ctx, out, budget,
            transactions::Draft{
                .source = collector,
                .destination = dst,
                .amount = math::amounts::kFraudCycle.sample(rng),
                .timestamp = time::toEpochSeconds(ts),
                .isFraud = 1,
                .ringId = static_cast<std::int32_t>(plan.ringId),
                .channel = outChannel,
            })) {
      break;
    }
  }

  return out;
}

} // namespace PhantomLedger::transfers::fraud::typologies::funnel
