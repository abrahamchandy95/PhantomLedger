#include "phantomledger/transfers/fraud/typologies/bipartite.hpp"

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

namespace PhantomLedger::transfers::fraud::typologies::bipartite {

namespace {

constexpr std::int64_t kSourceCountLo = 3;
constexpr std::int64_t kSourceCountHi = 8;

constexpr std::int64_t kDestCountLo = 3;
constexpr std::int64_t kDestCountHi = 8;

constexpr double kEdgeProbability = 0.55;
constexpr double kAmountFloor = 50.0;

struct Sides {
  std::vector<entity::Key> sources;
  std::vector<entity::Key> destinations;
};

[[nodiscard]] const std::vector<entity::Key> &
chooseSourcePool(const Plan &plan) noexcept {
  return !plan.muleAccounts.empty() ? plan.muleAccounts : plan.fraudAccounts;
}

[[nodiscard]] bool hasViableShape(const Plan &plan) noexcept {
  const auto &sourcePool = chooseSourcePool(plan);
  return sourcePool.size() >= 2 && plan.fraudAccounts.size() >= 2;
}

[[nodiscard]] std::size_t pickCount(random::Rng &rng, std::int64_t lo,
                                    std::int64_t hi, std::size_t maxAvailable) {
  const auto req = static_cast<std::size_t>(rng.uniformInt(lo, hi + 1));
  return std::min(req, maxAvailable);
}

[[nodiscard]] Sides pickSides(random::Rng &rng, const Plan &plan) {
  const auto &sourcePool = chooseSourcePool(plan);
  const auto m =
      pickCount(rng, kSourceCountLo, kSourceCountHi, sourcePool.size());
  const auto n =
      pickCount(rng, kDestCountLo, kDestCountHi, plan.fraudAccounts.size());
  return Sides{
      .sources = typologies::pickK(rng, sourcePool, m),
      .destinations = typologies::pickK(rng, plan.fraudAccounts, n),
  };
}

[[nodiscard]] double sampleAmount(random::Rng &rng) noexcept {
  return std::max(kAmountFloor, math::amounts::kFraud.sample(rng));
}

bool emitEdge(IllicitContext &ctx, std::vector<transactions::Transaction> &out,
              std::int32_t budget, const entity::Key &src,
              const entity::Key &dst, channels::Tag channel,
              std::uint32_t ringId, std::int32_t chainId,
              const BurstWindow &burst) {
  random::Rng &rng = *ctx.execution.rng;
  const double amt = primitives::utils::roundMoney(sampleAmount(rng));
  const auto ts = sampleTimestamp(rng, burst.baseDate, burst.durationDays,
                                  HourRange{.min = 0, .max = 24});

  return typologies::appendBoundedTxn(
      ctx, out, budget,
      transactions::Draft{
          .source = src,
          .destination = dst,
          .amount = amt,
          .timestamp = time::toEpochSeconds(ts),
          .isFraud = 1,
          .ringId = static_cast<std::int32_t>(ringId),
          .channel = channel,
          .chainId = chainId,
      });
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

  const auto sides = pickSides(rng, plan);
  if (sides.sources.size() < 2 || sides.destinations.size() < 2) {
    return out;
  }

  const auto channel = channels::tag(channels::Fraud::bipartite);
  const auto chainId = ctx.allocateChainId();

  for (const auto &src : sides.sources) {
    if (static_cast<std::int32_t>(out.size()) >= budget) {
      break;
    }
    for (const auto &dst : sides.destinations) {
      if (static_cast<std::int32_t>(out.size()) >= budget) {
        break;
      }
      if (src == dst) {
        continue;
      }
      if (!rng.coin(kEdgeProbability)) {
        continue;
      }
      if (!emitEdge(ctx, out, budget, src, dst, channel, plan.ringId, chainId,
                    burst)) {
        return out;
      }
    }
  }

  return out;
}

} // namespace PhantomLedger::transfers::fraud::typologies::bipartite
