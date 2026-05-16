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

// Per-hop haircut bounds (fraction of principal lost).
constexpr double kHopHaircutMin = 0.02;
constexpr double kHopHaircutRange = 0.06; // → [0.02, 0.08)

// Final integration haircut bounds.
constexpr double kFinalHaircutMin = 0.02;
constexpr double kFinalHaircutRange = 0.04; // → [0.02, 0.06)

// Delay between successive hops, in minutes.
constexpr std::int64_t kHopDelayMinutesLo = 15;
constexpr std::int64_t kHopDelayMinutesHi = 360; // up to 6h between hops

// Delay between chain exit and integration, in minutes.
constexpr std::int64_t kFinalDelayMinutesLo = 30;
constexpr std::int64_t kFinalDelayMinutesHi = 720; // up to 12h

// Minimum surviving principal before we abandon the chain.
constexpr double kPrincipalFloor = 10.0;

[[nodiscard]] bool containsKey(const std::vector<entity::Key> &haystack,
                               const entity::Key &needle) noexcept {
  return std::find(haystack.begin(), haystack.end(), needle) != haystack.end();
}

// Try to surface a mule to the front of the chain and a fraud account to the
// back, so the chain reads as: mule (entry) → … → fraud (ultimate beneficiary).
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

  // Pool of accounts that can serve as chain nodes (mules + frauds).
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

  // Each victim funds its own layering event with its own chain, principal,
  // and causal timeline. Each event gets a fresh chainId so a downstream
  // detector can group its transactions as one origin→beneficiary flow.
  for (const auto &victim : plan.victimAccounts) {
    if (static_cast<std::int32_t>(out.size()) >= budget) {
      break;
    }
    if (!rng.coin(0.60)) {
      continue;
    }

    // Sample chain length for this event.
    const auto requestedHops = static_cast<std::size_t>(rng.uniformInt(
        rules.minHops, static_cast<std::int64_t>(rules.maxHops) + 1));
    const auto chainLen = std::min(requestedHops, intermediaries.size());
    if (chainLen < 3) {
      continue;
    }

    auto chain = typologies::pickK(rng, intermediaries, chainLen);
    biasChainRoles(chain, plan.muleAccounts, plan.fraudAccounts);

    const auto &entry = chain.front();
    const auto &exitAcct = chain.back();

    // Sample a principal and a starting timestamp inside the burst window.
    double principal = math::amounts::kFraud.sample(rng);
    if (principal < 50.0) {
      principal = 50.0;
    }

    auto currentTs = sampleTimestamp(rng, burst.baseDate, burst.durationDays,
                                     HourRange{.min = 8, .max = 22});

    // One chain id per layering event. All transactions emitted below share
    // it, so a downstream consumer can reassemble the chain by chainId.
    const auto chainId = ctx.allocateChainId();

    // ─── Origination: victim → entry, full principal ────────────────
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

    // ─── Hops: principal decays, timestamps strictly increase ───────
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

      // Causal delay between hops: minutes to a few hours.
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

    // ─── Integration: exit → cashout (ideally a fraud account ≠ exit) ──
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
