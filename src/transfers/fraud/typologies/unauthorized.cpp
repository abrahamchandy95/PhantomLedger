#include "phantomledger/transfers/fraud/typologies/unauthorized.hpp"

#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/fraud/typologies/common.hpp"

#include <cmath>

namespace PhantomLedger::transfers::fraud::typologies::unauthorized {

namespace {

constexpr double kTestChargeLo = 0.50;
constexpr double kTestChargeRange = 2.50;
constexpr double kCardSpendLo = 35.0;
constexpr double kCardSpendRange = 865.0;
constexpr double kAtoDrainLo = 150.0;
constexpr double kAtoDrainRange = 3650.0;

[[nodiscard]] double cents(double v) noexcept {
  return std::round(v * 100.0) / 100.0;
}

} // namespace

std::vector<transactions::Transaction>
generate(IllicitContext &ctx, std::span<const CompromisePlan> plans,
         std::int32_t budget) {
  std::vector<transactions::Transaction> out;
  if (budget <= 0 || plans.empty()) {
    return out;
  }
  out.reserve(static_cast<std::size_t>(budget));

  random::Rng &rng = *ctx.execution.rng;

  for (const auto &plan : plans) {
    for (std::int32_t e = 0; e < plan.targetEvents; ++e) {
      const auto offset = static_cast<std::int64_t>(
          rng.nextDouble() * static_cast<double>(plan.spanSeconds));

      transactions::Draft draft{};
      draft.source = plan.victimAccount;
      draft.timestamp = plan.startTs + offset;
      draft.isFraud = 1;
      draft.ringId = -1;

      if (plan.cardRail) {
        draft.destination = pickOne(rng, ctx.billerAccounts);
        draft.channel = channels::tag(channels::Legit::cardPurchase);
        const bool testCharge = (e < 2) && rng.coin(0.7);
        draft.amount =
            testCharge
                ? cents(kTestChargeLo + kTestChargeRange * rng.nextDouble())
                : cents(kCardSpendLo + kCardSpendRange * rng.nextDouble());
      } else {
        draft.destination = plan.dropAccount;
        draft.channel = channels::tag(channels::Legit::p2p);
        draft.amount = cents(kAtoDrainLo + kAtoDrainRange * rng.nextDouble());
      }

      if (!appendBoundedTxn(ctx, out, budget, draft)) {
        return out;
      }
      out.back().session.deviceId = plan.device;
      out.back().session.ipAddress = plan.ip;
    }
  }
  return out;
}

} // namespace PhantomLedger::transfers::fraud::typologies::unauthorized
