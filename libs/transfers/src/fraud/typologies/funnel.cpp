#include "phantomledger/transfers/fraud/typologies/funnel.hpp"

#include "phantomledger/math/amounts.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/fraud/schedule.hpp"
#include "phantomledger/transfers/fraud/typologies/classic.hpp"
#include "phantomledger/transfers/fraud/typologies/common.hpp"

#include <algorithm>
#include <cstdint>

namespace PhantomLedger::transfers::fraud::typologies::funnel {

namespace {

// Fraction of burst dedicated to the inbound (gather) phase.
constexpr std::int32_t kInboundPhaseNum = 6;
constexpr std::int32_t kInboundPhaseDen = 10;

// Probability a given source feeds into the collector.
constexpr double kFeedProbability = 0.55;

// Outbound-phase delays (relative to latest inbound) in hours.
constexpr std::int64_t kInitialOutDelayHoursLo = 1;
constexpr std::int64_t kInitialOutDelayHoursHi = 12;

// Per-cashout delay between outbound transactions, in minutes.
constexpr std::int64_t kInterOutDelayMinutesLo = 5;
constexpr std::int64_t kInterOutDelayMinutesHi = 240;

// Aggregate haircut applied to the collected pool before scattering.
constexpr double kCollectorHaircutMin = 0.05;
constexpr double kCollectorHaircutRange = 0.05; // → [0.05, 0.10)

// Minimum survivable outbound amount before we abandon the funnel.
constexpr double kAmountFloor = 5.0;

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
                                           .tailPaddingDays = 10,
                                           .minDays = 2,
                                           .maxDays = 6,
                                       });

  const auto pool = plan.participantAccounts();
  if (pool.size() < 2) {
    return classic::generate(ctx, plan, budget);
  }

  const auto &collector = pool[rng.choiceIndex(pool.size())];

  // Build a cashout set that EXCLUDES the collector to avoid self-loops, then
  // fall back to fraud accounts if exclusion leaves us empty.
  auto cashouts =
      typologies::pickK(rng, pool, std::min<std::size_t>(4, pool.size()));
  cashouts.erase(std::remove(cashouts.begin(), cashouts.end(), collector),
                 cashouts.end());
  if (cashouts.empty()) {
    if (!plan.fraudAccounts.empty()) {
      for (int attempt = 0; attempt < 6 && cashouts.empty(); ++attempt) {
        const auto &candidate =
            plan.fraudAccounts[rng.choiceIndex(plan.fraudAccounts.size())];
        if (candidate != collector) {
          cashouts.push_back(candidate);
        }
      }
    }
    if (cashouts.empty()) {
      return classic::generate(ctx, plan, budget);
    }
  }

  const auto inChannel = channels::tag(channels::Fraud::funnelIn);
  const auto outChannel = channels::tag(channels::Fraud::funnelOut);

  std::vector<entity::Key> sources;
  sources.reserve(plan.victimAccounts.size() + plan.muleAccounts.size());
  sources.insert(sources.end(), plan.victimAccounts.begin(),
                 plan.victimAccounts.end());
  sources.insert(sources.end(), plan.muleAccounts.begin(),
                 plan.muleAccounts.end());

  // One chain id for the entire gather → scatter operation. Gather and
  // scatter are different shapes (N→1 and 1→M) but they're a single analytic
  // unit linked by the collector, so they share a chain id.
  const auto chainId = ctx.allocateChainId();

  // ─── Phase 1: GATHER — sources → collector ──────────────────────────
  const std::int32_t inboundDays = std::max<std::int32_t>(
      1, (burst.durationDays * kInboundPhaseNum) / kInboundPhaseDen);

  double totalIn = 0.0;
  time::TimePoint latestInTs = burst.baseDate;
  bool anyInbound = false;

  for (const auto &src : sources) {
    if (static_cast<std::int32_t>(out.size()) >= budget) {
      break;
    }
    if (src == collector) {
      continue;
    }
    if (!rng.coin(kFeedProbability)) {
      continue;
    }

    const auto ts = sampleTimestamp(rng, burst.baseDate, inboundDays,
                                    HourRange{.min = 8, .max = 22});
    const double amt = primitives::utils::floorAndRound(
        math::amounts::kFraud.sample(rng), 50.0);

    if (!typologies::appendBoundedTxn(
            ctx, out, budget,
            transactions::Draft{
                .source = src,
                .destination = collector,
                .amount = amt,
                .timestamp = time::toEpochSeconds(ts),
                .isFraud = 1,
                .ringId = static_cast<std::int32_t>(plan.ringId),
                .channel = inChannel,
                .chainId = chainId,
            })) {
      break;
    }

    totalIn += amt;
    anyInbound = true;
    if (ts > latestInTs) {
      latestInTs = ts;
    }
  }

  if (!anyInbound || totalIn <= 0.0) {
    return out;
  }

  // ─── Phase 2: SCATTER — collector → cashouts, after the gather ─────
  const double haircut =
      kCollectorHaircutMin + kCollectorHaircutRange * rng.nextDouble();
  double remaining = totalIn * (1.0 - haircut);

  const auto initialDelayHours =
      rng.uniformInt(kInitialOutDelayHoursLo, kInitialOutDelayHoursHi + 1);
  time::TimePoint currentTs = latestInTs + time::Hours{initialDelayHours};

  const auto outboundCount = static_cast<std::int32_t>(rng.uniformInt(6, 17));

  for (std::int32_t i = 0; i < outboundCount; ++i) {
    if (static_cast<std::int32_t>(out.size()) >= budget) {
      break;
    }
    if (remaining < kAmountFloor) {
      break;
    }

    const std::int32_t remainingSteps = outboundCount - i;
    double amt;
    if (remainingSteps == 1) {
      amt = remaining;
    } else {
      const double avg = remaining / static_cast<double>(remainingSteps);
      const double frac = 0.6 + 0.7 * rng.nextDouble();
      amt = avg * frac;
      const double cap = remaining * 0.8;
      if (amt > cap) {
        amt = cap;
      }
    }
    amt = primitives::utils::roundMoney(amt);
    if (amt < kAmountFloor) {
      break;
    }

    const auto interDelayMin =
        rng.uniformInt(kInterOutDelayMinutesLo, kInterOutDelayMinutesHi + 1);
    currentTs = currentTs + time::Minutes{interDelayMin};

    const auto &dst = cashouts[rng.choiceIndex(cashouts.size())];

    if (!typologies::appendBoundedTxn(
            ctx, out, budget,
            transactions::Draft{
                .source = collector,
                .destination = dst,
                .amount = amt,
                .timestamp = time::toEpochSeconds(currentTs),
                .isFraud = 1,
                .ringId = static_cast<std::int32_t>(plan.ringId),
                .channel = outChannel,
                .chainId = chainId,
            })) {
      break;
    }

    remaining -= amt;
  }

  return out;
}

} // namespace PhantomLedger::transfers::fraud::typologies::funnel
