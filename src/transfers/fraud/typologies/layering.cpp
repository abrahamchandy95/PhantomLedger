#include "phantomledger/transfers/fraud/typologies/layering.hpp"

#include "phantomledger/math/amounts.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/fraud/schedule.hpp"
#include "phantomledger/transfers/fraud/typologies/classic.hpp"
#include "phantomledger/transfers/fraud/typologies/common.hpp"

#include <algorithm>

namespace PhantomLedger::transfers::fraud::typologies::layering {

std::vector<transactions::Transaction> generate(IllicitContext &ctx,
                                                const Plan &plan,
                                                std::int32_t budget,
                                                const Rules &rules) {
  std::vector<transactions::Transaction> out;
  if (budget <= 0) {
    return out;
  }

  random::Rng &rng = *ctx.execution.rng;

  const auto burst =
      sampleBurstWindow(rng, ctx.window.startDate, ctx.window.days,
                        BurstShape{
                            .tailPaddingDays = 10,
                            .minDays = 3,
                            .maxDays = 7,
                        });

  std::vector<entity::Key> nodes;
  nodes.reserve(plan.muleAccounts.size() + plan.fraudAccounts.size());
  nodes.insert(nodes.end(), plan.muleAccounts.begin(), plan.muleAccounts.end());
  nodes.insert(nodes.end(), plan.fraudAccounts.begin(),
               plan.fraudAccounts.end());

  if (nodes.size() < 3 || plan.victimAccounts.empty()) {
    return classic::generate(ctx, plan, budget);
  }

  const auto hops = static_cast<std::size_t>(rng.uniformInt(
      rules.minHops, static_cast<std::int64_t>(rules.maxHops) + 1));

  auto chain = typologies::pickK(rng, nodes, std::min(hops, nodes.size()));
  while (chain.size() < hops) {
    chain.push_back(nodes[rng.choiceIndex(nodes.size())]);
  }

  const auto &entry = chain.front();
  const auto &exitAcct = chain.back();

  const auto inChannel = channels::tag(channels::Fraud::layeringIn);
  const auto hopChannel = channels::tag(channels::Fraud::layeringHop);
  const auto outChannel = channels::tag(channels::Fraud::layeringOut);

  // ---- Phase 1: victim → entry (60% per victim) -----------------------
  for (const auto &victim : plan.victimAccounts) {
    if (static_cast<std::int32_t>(out.size()) >= budget) {
      break;
    }
    if (!rng.coin(0.60)) {
      continue;
    }

    const auto ts = sampleTimestamp(rng, burst.baseDate, burst.durationDays,
                                    HourRange{.min = 8, .max = 22});
    if (!typologies::appendBoundedTxn(
            ctx, out, budget,
            transactions::Draft{
                .source = victim,
                .destination = entry,
                .amount = math::amounts::kFraud.sample(rng),
                .timestamp = time::toEpochSeconds(ts),
                .isFraud = 1,
                .ringId = static_cast<std::int32_t>(plan.ringId),
                .channel = inChannel,
            })) {
      break;
    }
  }

  // ---- Phase 2: chain hops (1–3 transfers per edge) ------------------
  for (std::size_t i = 0; i + 1 < chain.size(); ++i) {
    if (static_cast<std::int32_t>(out.size()) >= budget) {
      break;
    }
    const auto &src = chain[i];
    const auto &dst = chain[i + 1];

    const auto edgeReps = static_cast<std::int32_t>(rng.uniformInt(1, 5));
    for (std::int32_t r = 0; r < edgeReps; ++r) {
      if (static_cast<std::int32_t>(out.size()) >= budget) {
        break;
      }

      const auto ts = sampleTimestamp(rng, burst.baseDate, burst.durationDays,
                                      HourRange{.min = 0, .max = 24});
      if (!typologies::appendBoundedTxn(
              ctx, out, budget,
              transactions::Draft{
                  .source = src,
                  .destination = dst,
                  .amount = math::amounts::kFraudCycle.sample(rng),
                  .timestamp = time::toEpochSeconds(ts),
                  .isFraud = 1,
                  .ringId = static_cast<std::int32_t>(plan.ringId),
                  .channel = hopChannel,
              })) {
        break;
      }
    }
  }

  // ---- Phase 3: exit → cashout (single optional emission) ------------
  if (static_cast<std::int32_t>(out.size()) < budget) {
    const auto &cashout =
        !plan.fraudAccounts.empty()
            ? plan.fraudAccounts[rng.choiceIndex(plan.fraudAccounts.size())]
            : nodes[rng.choiceIndex(nodes.size())];

    const auto ts = sampleTimestamp(rng, burst.baseDate, burst.durationDays,
                                    HourRange{.min = 0, .max = 24});
    (void)typologies::appendBoundedTxn(
        ctx, out, budget,
        transactions::Draft{
            .source = exitAcct,
            .destination = cashout,
            .amount = math::amounts::kFraud.sample(rng),
            .timestamp = time::toEpochSeconds(ts),
            .isFraud = 1,
            .ringId = static_cast<std::int32_t>(plan.ringId),
            .channel = outChannel,
        });
  }

  return out;
}

} // namespace PhantomLedger::transfers::fraud::typologies::layering
