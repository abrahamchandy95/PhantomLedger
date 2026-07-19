#include "phantomledger/transfers/fraud/typologies/structuring.hpp"

#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/fraud/schedule.hpp"
#include "phantomledger/transfers/fraud/typologies/common.hpp"

#include <algorithm>
#include <cstdint>

namespace PhantomLedger::transfers::fraud::typologies::structuring {

namespace {

// Mix of smurf amount profiles. Probabilities should sum to 1.0.
constexpr double kThresholdProfileP = 0.60;
constexpr double kMediumProfileP = 0.25;
// Remainder (~0.15) is the "small" profile.

constexpr double kMediumAmountLo = 3'000.0;
constexpr double kMediumAmountRange = 4'000.0; // → [3k, 7k)

constexpr double kSmallAmountLo = 300.0;
constexpr double kSmallAmountRange = 1'200.0; // → [300, 1500)

// Probability of using a secondary target for a given split.
constexpr double kSecondaryTargetP = 0.20;

constexpr double kAmountFloor = 50.0;

[[nodiscard]] double sampleSmurfAmount(random::Rng &rng,
                                       const Rules &rules) noexcept {
  const double u = rng.nextDouble();
  if (u < kThresholdProfileP) {
    const double epsRange = rules.epsilonMax - rules.epsilonMin;
    const double eps = rules.epsilonMin + epsRange * rng.nextDouble();
    return std::max(kAmountFloor, rules.threshold - eps);
  }
  if (u < kThresholdProfileP + kMediumProfileP) {
    return kMediumAmountLo + kMediumAmountRange * rng.nextDouble();
  }
  return kSmallAmountLo + kSmallAmountRange * rng.nextDouble();
}

} // namespace

std::vector<transactions::Transaction> generate(IllicitContext &ctx,
                                                const Plan &plan,
                                                std::int32_t budget,
                                                const Rules &rules) {
  std::vector<transactions::Transaction> out;
  if (budget <= 0) {
    return out;
  }
  if (plan.muleAccounts.empty() && plan.fraudAccounts.empty()) {
    return out;
  }

  random::Rng &rng = *ctx.execution.rng;

  const auto burst = sampleBurstWindow(rng, ctx.window.start, ctx.window.days,
                                       BurstShape{
                                           .tailPaddingDays = 10,
                                           .minDays = 3,
                                           .maxDays = 8,
                                       });

  const auto &primaryTarget =
      !plan.muleAccounts.empty() ? typologies::pickOne(rng, plan.muleAccounts)
                                 : typologies::pickOne(rng, plan.fraudAccounts);

  entity::Key secondaryTarget = primaryTarget;
  {
    const auto &altPool =
        !plan.fraudAccounts.empty() ? plan.fraudAccounts : plan.muleAccounts;
    for (int attempt = 0; attempt < 6 && secondaryTarget == primaryTarget;
         ++attempt) {
      secondaryTarget = altPool[rng.choiceIndex(altPool.size())];
    }
  }
  const bool hasSecondary = (secondaryTarget != primaryTarget);

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

    const std::int32_t subBurstSpan = std::max<std::int32_t>(
        1, std::min<std::int32_t>(2, burst.durationDays));
    const std::int32_t subBurstOffset = static_cast<std::int32_t>(
        rng.uniformInt(0, static_cast<std::int64_t>(
                              std::max(1, burst.durationDays - subBurstSpan)) +
                              1));
    const auto victimBaseDate = burst.baseDate + time::Days{subBurstOffset};

    // One chain id per victim's batch of splits. The whole "smurf attack"
    // from one victim is one analytic unit.
    const auto chainId = ctx.allocateChainId();

    for (std::int32_t i = 0; i < splits; ++i) {
      if (static_cast<std::int32_t>(out.size()) >= budget) {
        break;
      }

      const double amount =
          primitives::utils::roundMoney(sampleSmurfAmount(rng, rules));

      const auto ts = sampleTimestamp(rng, victimBaseDate, subBurstSpan,
                                      HourRange{.min = 8, .max = 22});

      const entity::Key &target = (hasSecondary && rng.coin(kSecondaryTargetP))
                                      ? secondaryTarget
                                      : primaryTarget;

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
                  .chainId = chainId,
              })) {
        break;
      }
    }
  }

  return out;
}

} // namespace PhantomLedger::transfers::fraud::typologies::structuring
