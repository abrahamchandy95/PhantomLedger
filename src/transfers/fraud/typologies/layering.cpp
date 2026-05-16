#include "phantomledger/transfers/fraud/typologies/layering.hpp"

#include "phantomledger/math/amounts.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/fraud/schedule.hpp"
#include "phantomledger/transfers/fraud/typologies/classic.hpp"
#include "phantomledger/transfers/fraud/typologies/common.hpp"

#include <algorithm>
#include <cstdint>

namespace PhantomLedger::transfers::fraud::typologies::layering {

namespace {

constexpr double kHopHaircutMin = 0.02;
constexpr double kHopHaircutRange = 0.06;

constexpr double kFinalHaircutMin = 0.02;
constexpr double kFinalHaircutRange = 0.04;

constexpr std::int64_t kHopDelayMinutesLo = 15;
constexpr std::int64_t kHopDelayMinutesHi = 360;

constexpr std::int64_t kFinalDelayMinutesLo = 30;
constexpr std::int64_t kFinalDelayMinutesHi = 720;

constexpr double kPrincipalFloor = 10.0;

constexpr double kShellBiasProbability = 0.65;

[[nodiscard]] bool containsKey(const std::vector<entity::Key> &haystack,
                               const entity::Key &needle) noexcept {
  return std::find(haystack.begin(), haystack.end(), needle) != haystack.end();
}

void biasChainRoles(std::vector<entity::Key> &chain,
                    const std::vector<entity::Key> &mules,
                    const std::vector<entity::Key> &frauds) {
  if (chain.size() < 2) {
    return;
  }
  if (!containsKey(mules, chain.front())) {
    for (std::size_t i = 1; i < chain.size(); ++i) {
      if (containsKey(mules, chain[i])) {
        std::swap(chain.front(), chain[i]);
        break;
      }
    }
  }
  if (!containsKey(frauds, chain.back())) {
    for (std::size_t i = 0; i + 1 < chain.size(); ++i) {
      if (containsKey(frauds, chain[i])) {
        std::swap(chain.back(), chain[i]);
        break;
      }
    }
  }
}

void swapMiddleForShells(std::vector<entity::Key> &chain,
                         const std::vector<entity::Key> &shells,
                         random::Rng &rng) {
  if (chain.size() < 3 || shells.empty()) {
    return;
  }
  for (std::size_t i = 1; i + 1 < chain.size(); ++i) {
    if (!rng.coin(kShellBiasProbability)) {
      continue;
    }
    const auto &candidate = shells[rng.choiceIndex(shells.size())];
    if (containsKey(chain, candidate)) {
      continue;
    }
    chain[i] = candidate;
  }
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

  random::Rng &rng = *ctx.execution.rng;

  const auto burst = sampleBurstWindow(rng, ctx.window.start, ctx.window.days,
                                       BurstShape{
                                           .tailPaddingDays = 10,
                                           .minDays = 3,
                                           .maxDays = 7,
                                       });

  std::vector<entity::Key> intermediaries;
  intermediaries.reserve(plan.muleAccounts.size() + plan.fraudAccounts.size());
  intermediaries.insert(intermediaries.end(), plan.muleAccounts.begin(),
                        plan.muleAccounts.end());
  intermediaries.insert(intermediaries.end(), plan.fraudAccounts.begin(),
                        plan.fraudAccounts.end());

  if (intermediaries.size() < 3 || plan.victimAccounts.empty()) {
    return classic::generate(ctx, plan, budget);
  }

  const auto inChannel = channels::tag(channels::Fraud::layeringIn);
  const auto hopChannel = channels::tag(channels::Fraud::layeringHop);
  const auto outChannel = channels::tag(channels::Fraud::layeringOut);

  for (const auto &victim : plan.victimAccounts) {
    if (static_cast<std::int32_t>(out.size()) >= budget) {
      break;
    }
    if (!rng.coin(0.60)) {
      continue;
    }

    const auto requestedHops = static_cast<std::size_t>(rng.uniformInt(
        rules.minHops, static_cast<std::int64_t>(rules.maxHops) + 1));
    const auto chainLen = std::min(requestedHops, intermediaries.size());
    if (chainLen < 3) {
      continue;
    }

    auto chain = typologies::pickK(rng, intermediaries, chainLen);
    biasChainRoles(chain, plan.muleAccounts, plan.fraudAccounts);
    swapMiddleForShells(chain, plan.shellFraudAccounts, rng);

    const auto &entry = chain.front();
    const auto &exitAcct = chain.back();

    double principal = math::amounts::kFraud.sample(rng);
    if (principal < 50.0) {
      principal = 50.0;
    }

    auto currentTs = sampleTimestamp(rng, burst.baseDate, burst.durationDays,
                                     HourRange{.min = 8, .max = 22});

    const auto chainId = ctx.allocateChainId();

    if (!typologies::appendBoundedTxn(
            ctx, out, budget,
            transactions::Draft{
                .source = victim,
                .destination = entry,
                .amount = primitives::utils::roundMoney(principal),
                .timestamp = time::toEpochSeconds(currentTs),
                .isFraud = 1,
                .ringId = static_cast<std::int32_t>(plan.ringId),
                .channel = inChannel,
                .chainId = chainId,
            })) {
      break;
    }

    bool chainBroken = false;
    bool budgetBlown = false;
    double current = principal;
    for (std::size_t i = 0; i + 1 < chain.size(); ++i) {
      if (static_cast<std::int32_t>(out.size()) >= budget) {
        budgetBlown = true;
        break;
      }

      const double haircut =
          kHopHaircutMin + kHopHaircutRange * rng.nextDouble();
      current = current * (1.0 - haircut);
      if (current < kPrincipalFloor) {
        chainBroken = true;
        break;
      }

      const auto delayMin =
          rng.uniformInt(kHopDelayMinutesLo, kHopDelayMinutesHi + 1);
      currentTs = currentTs + time::Minutes{delayMin};

      if (!typologies::appendBoundedTxn(
              ctx, out, budget,
              transactions::Draft{
                  .source = chain[i],
                  .destination = chain[i + 1],
                  .amount = primitives::utils::roundMoney(current),
                  .timestamp = time::toEpochSeconds(currentTs),
                  .isFraud = 1,
                  .ringId = static_cast<std::int32_t>(plan.ringId),
                  .channel = hopChannel,
                  .chainId = chainId,
              })) {
        budgetBlown = true;
        break;
      }
    }

    if (budgetBlown) {
      break;
    }
    if (chainBroken) {
      continue;
    }

    if (static_cast<std::int32_t>(out.size()) >= budget) {
      break;
    }
    if (plan.fraudAccounts.empty()) {
      continue;
    }

    entity::Key cashout =
        plan.fraudAccounts[rng.choiceIndex(plan.fraudAccounts.size())];
    for (int attempt = 0;
         attempt < 4 && cashout == exitAcct && plan.fraudAccounts.size() > 1;
         ++attempt) {
      cashout = plan.fraudAccounts[rng.choiceIndex(plan.fraudAccounts.size())];
    }

    const double finalHaircut =
        kFinalHaircutMin + kFinalHaircutRange * rng.nextDouble();
    current = current * (1.0 - finalHaircut);
    if (current < kPrincipalFloor) {
      continue;
    }

    const auto delayMin =
        rng.uniformInt(kFinalDelayMinutesLo, kFinalDelayMinutesHi + 1);
    currentTs = currentTs + time::Minutes{delayMin};

    (void)typologies::appendBoundedTxn(
        ctx, out, budget,
        transactions::Draft{
            .source = exitAcct,
            .destination = cashout,
            .amount = primitives::utils::roundMoney(current),
            .timestamp = time::toEpochSeconds(currentTs),
            .isFraud = 1,
            .ringId = static_cast<std::int32_t>(plan.ringId),
            .channel = outChannel,
            .chainId = chainId,
        });
  }

  return out;
}

} // namespace PhantomLedger::transfers::fraud::typologies::layering
