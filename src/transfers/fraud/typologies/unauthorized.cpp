#include "phantomledger/transfers/fraud/typologies/unauthorized.hpp"

#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/fraud/typologies/amounts.hpp"
#include "phantomledger/transfers/fraud/typologies/common.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace PhantomLedger::transfers::fraud::typologies::unauthorized {

namespace {

// Per-plan event schedule.
//
// Card-compromise rail (two-phase; tests PROVABLY precede spend):
//   phase 1: settled card-testing micro-charges inside a 10-60 minute
//            window at the start of the compromise span
//            (tWindow = clamp(span/6, 600s, 3600s));
//   phase 2: escalated spend, beginning at least one hour after the
//            test window opens (spendStart = tWindow + max(3600, span/6))
//            and running to the end of the span.
//   The two offset ranges are disjoint by construction, so a validated
//   test can never carry a later timestamp than the spend it validated
//   (the original implementation drew every offset uniformly over the
//   whole span, which allowed exactly that inversion).
//   Real-world anchor: validated cards escalate to larger purchases
//   within hours to days (see citations in amounts.hpp).
//
// Account-takeover rail: p2p drains spread over the (2-32h) compromise
//   span, emitted in ascending-timestamp order.
//
// Determinism: draws come only from ctx.execution.rng, in a fixed,
// documented order per plan:
//   card rail: coin(0.7) per test-candidate slot (at most 2), then per
//              event in slot order: 1 uniform (timestamp) + 2 uniforms
//              (amount);
//   ato rail:  per event: 1 uniform (timestamp) + 2 uniforms (amount).
// Events are then stable-sorted by timestamp (pairs stay paired) and
// emitted chronologically; card-rail biller destinations draw one
// further uniform per emitted event, in emission order. The stream is
// therefore a pure function of the incoming Rng state.

struct Event {
  std::int64_t ts = 0;
  double amount = 0.0;
};

[[nodiscard]] std::int64_t offsetIn(random::Rng &rng, std::int64_t lo,
                                    std::int64_t hi) {
  // Uniform integer offset in [lo, hi); consumes exactly one uniform.
  if (hi <= lo) {
    return lo;
  }
  const auto width = static_cast<double>(hi - lo);
  auto off = lo + static_cast<std::int64_t>(rng.nextDouble() * width);
  return std::min(off, hi - 1);
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

  std::vector<Event> events;
  std::vector<bool> isTest;

  for (const auto &plan : plans) {
    const auto span =
        static_cast<std::int64_t>(std::max<std::int32_t>(plan.spanSeconds, 2));
    const std::int32_t target = plan.targetEvents;
    if (target <= 0) {
      continue;
    }

    events.clear();
    isTest.assign(static_cast<std::size_t>(target), false);

    if (plan.cardRail) {
      // Phase boundaries (all offsets relative to plan.startTs).
      const std::int64_t tWindow =
          std::clamp<std::int64_t>(span / 6, 600, 3600);
      const std::int64_t spendStart =
          std::min(tWindow + std::max<std::int64_t>(3600, span / 6), span - 1);

      // Test-candidate decisions first (slot order), then event draws.
      for (std::int32_t e = 0; e < target && e < 2; ++e) {
        isTest[static_cast<std::size_t>(e)] = rng.coin(0.7);
      }
      for (std::int32_t e = 0; e < target; ++e) {
        Event ev{};
        if (isTest[static_cast<std::size_t>(e)]) {
          ev.ts = plan.startTs + offsetIn(rng, 0, tWindow);
          ev.amount = amounts::cardTestCharge(rng);
        } else {
          ev.ts = plan.startTs + offsetIn(rng, spendStart, span);
          ev.amount = amounts::cardFraudSpend(rng);
        }
        events.push_back(ev);
      }
    } else {
      for (std::int32_t e = 0; e < target; ++e) {
        Event ev{};
        ev.ts = plan.startTs + offsetIn(rng, 0, span);
        ev.amount = amounts::atoDrainAmount(rng);
        events.push_back(ev);
      }
    }

    std::stable_sort(
        events.begin(), events.end(),
        [](const Event &a, const Event &b) { return a.ts < b.ts; });

    for (const Event &ev : events) {
      transactions::Draft draft{};
      draft.source = plan.victimAccount;
      draft.timestamp = ev.ts;
      draft.amount = ev.amount;
      draft.isFraud = 1;
      draft.ringId = -1;

      if (plan.cardRail) {
        draft.destination = pickOne(rng, ctx.billerAccounts);
        draft.channel = channels::tag(channels::Legit::cardPurchase);
      } else {
        draft.destination = plan.dropAccount;
        draft.channel = channels::tag(channels::Legit::p2p);
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
