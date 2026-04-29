#include "phantomledger/transfers/fraud/typologies/invoice.hpp"

#include "phantomledger/probability/distributions/lognormal.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/fraud/typologies/common.hpp"

#include <algorithm>
#include <cmath>

namespace PhantomLedger::transfers::fraud::typologies::invoice {

std::vector<transactions::Transaction>
generate(IllicitContext &ctx, const Plan &plan, std::int32_t budget) {
  std::vector<transactions::Transaction> out;
  if (budget <= 0) {
    return out;
  }
  if (plan.fraudAccounts.empty() || ctx.billerAccounts.empty()) {
    return out;
  }

  random::Rng &rng = *ctx.execution.rng;

  const auto baseRange = std::max<std::int32_t>(1, ctx.window.days - 14);
  const auto baseOffsetDays = static_cast<std::int32_t>(
      rng.uniformInt(0, static_cast<std::int64_t>(baseRange) + 1));
  const auto baseDate = ctx.window.startDate + time::Days{baseOffsetDays};

  // weeks = max(1, min(6, days // 7))
  const auto weeks =
      std::max<std::int32_t>(1, std::min<std::int32_t>(6, ctx.window.days / 7));

  // Number of invoice events in this ring: rng.int(3, 10) → [3, 11).
  const auto events = static_cast<std::int32_t>(rng.uniformInt(3, 11));

  const auto channel = channels::tag(channels::Fraud::invoice);

  for (std::int32_t i = 0; i < events; ++i) {
    if (static_cast<std::int32_t>(out.size()) >= budget) {
      break;
    }

    const auto &src = typologies::pickOne(rng, plan.fraudAccounts);
    const auto &dst = typologies::pickOne(rng, ctx.billerAccounts);

    const double raw = probability::distributions::lognormal(rng, 8.0, 0.35);
    const double amount = std::round(raw / 10.0) * 10.0;

    // Day-of-burst: rng.int(0, weeks) inclusive → [0, weeks + 1).
    const auto weekIdx = static_cast<std::int32_t>(
        rng.uniformInt(0, static_cast<std::int64_t>(weeks) + 1));
    // Hour: rng.int(9, 18) → [9, 19); minute: rng.int(0, 60) → [0, 61).
    const auto hour = static_cast<std::int32_t>(rng.uniformInt(9, 19));
    const auto minute = static_cast<std::int32_t>(rng.uniformInt(0, 61));

    const auto ts = baseDate + time::Days{7 * weekIdx} + time::Hours{hour} +
                    time::Minutes{minute};

    if (!typologies::appendBoundedTxn(
            ctx, out, budget,
            transactions::Draft{
                .source = src,
                .destination = dst,
                .amount = amount,
                .timestamp = time::toEpochSeconds(ts),
                .isFraud = 1,
                .ringId = static_cast<std::int32_t>(plan.ringId),
                .channel = channel,
            })) {
      break;
    }
  }

  return out;
}

} // namespace PhantomLedger::transfers::fraud::typologies::invoice
