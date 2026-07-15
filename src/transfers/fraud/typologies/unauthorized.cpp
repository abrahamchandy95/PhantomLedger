#include "phantomledger/transfers/fraud/typologies/unauthorized.hpp"

#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/fraud/typologies/amounts.hpp"
#include "phantomledger/transfers/fraud/typologies/common.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace PhantomLedger::transfers::fraud::typologies::unauthorized {

namespace {

struct Event {
  std::int64_t ts = 0;
  double amount = 0.0;
};

[[nodiscard]] std::string_view renderUInt(std::array<char, 16> &buf,
                                          std::uint32_t value) noexcept {
  auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
  (void)ec;
  return std::string_view(buf.data(),
                          static_cast<std::size_t>(ptr - buf.data()));
}

[[nodiscard]] std::int64_t offsetIn(random::Rng &rng, std::int64_t lo,
                                    std::int64_t hi) {
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

  if (ctx.execution.factory == nullptr) {
    throw std::logic_error("unauthorized::generate requires the S9 "
                           "content-keyed RngFactory on Execution");
  }
  const auto &keyFactory = *ctx.execution.factory;
  std::array<char, 16> seqBuf{};

  std::vector<Event> events;
  std::vector<bool> isTest;

  for (const auto &plan : plans) {
    const auto span =
        static_cast<std::int64_t>(std::max<std::int32_t>(plan.spanSeconds, 2));
    const std::int32_t target = plan.targetEvents;
    if (target <= 0) {
      continue;
    }

    auto rng = keyFactory.rng(
        {"fraud", "unauth", "plan", renderUInt(seqBuf, plan.seq)});
    const auto planTxf = ctx.execution.txf.withRng(rng);

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

      if (!appendBoundedTxn(planTxf, out, budget, draft)) {
        return out;
      }
      out.back().session.deviceId = plan.device;
      out.back().session.ipAddress = plan.ip;
    }
  }
  return out;
}

} // namespace PhantomLedger::transfers::fraud::typologies::unauthorized
