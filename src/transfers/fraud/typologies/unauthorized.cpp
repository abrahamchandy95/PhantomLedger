#include "phantomledger/transfers/fraud/typologies/unauthorized.hpp"

#include "phantomledger/activity/spending/market/commerce/geo_pools.hpp"
#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/primitives/random/distributions/cdf.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/econ/nominal.hpp"
#include "phantomledger/synth/geo/catalog.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/fraud/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/fraud/typologies/amounts.hpp"
#include "phantomledger/transfers/fraud/typologies/common.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace PhantomLedger::transfers::fraud::typologies::unauthorized {

namespace {

inline constexpr std::int64_t kSecondsPerDay = 86'400;

// card-fraud-realism-v2 step b (contract docs/card_fraud_v2_roadmap.md,
// gate 1 of docs/card_fraud_online_gnn.md). THE MODALITY SPLIT for the
// stolen-card rail: card-not-present dominates real card fraud (remote
// purchases are where stolen credentials monetize), so the majority of
// compromises are CNP. DECLARED CHOICE — the DIRECTION is the anchored
// claim (CNP-majority), the exact split is a modeling choice pending a
// per-era card-fraud-modality series.
inline constexpr double kCardNotPresentShare = 0.70;

struct Event {
  std::int64_t ts = 0;
  double amount = 0.0;
  bool test = false;
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
  // Uniform integer offset in [lo, hi); consumes exactly one uniform.
  if (hi <= lo) {
    return lo;
  }
  const auto width = static_cast<double>(hi - lo);
  auto off = lo + static_cast<std::int64_t>(rng.nextDouble() * width);
  return std::min(off, hi - 1);
}

// H1 step 2b (class F): the event's CPI level for the continuous
// samplers (cardFraudSpend, atoDrainAmount). The denomination
// samplers (cardTestCharge, giftCardScamAmount) stay FIXED-NOMINAL —
// owner-approved lattice CHOICE, authority U-6.
[[nodiscard]] double eventPriceScale(std::int64_t epochSec) {
  return ::PhantomLedger::synth::econ::priceScale(
      ::PhantomLedger::time::toCalendarDate(
          ::PhantomLedger::time::fromEpochSeconds(epochSec))
          .year);
}

// THE P0 FIX (card-fraud-realism-v2 step b). Card-rail fraud settles
// against the MERCHANT ACCEPTANCE POPULATION — the same catalogue
// legitimate card purchases use — instead of the legit-TRANSFER
// biller/hub pool. Two disjoint destination populations were the
// merchant-identity shortcut: every fraud row landed on a merchant
// with zero legitimate card rows, so merchant id alone classified the
// corpus.
//
// Selection is MODALITY-CONDITIONED, which is what keeps the fix from
// trading one shortcut for another:
//
//   card-present  weighted over PHYSICAL outlets by the SAME kernel the
//                 spending session uses — popularity x exp(-miles /
//                 cardPresentDecayScaleMiles(home)) — so a stolen card
//                 is spent near where the cardholder lives, on the same
//                 geographic axis legitimate activity follows. A flat
//                 national draw here would have made "distance from
//                 home" the new giveaway.
//   card-not-present  weighted over the Footprint::online population by
//                 popularity alone; distance does not apply, and this
//                 is exactly the population legitimate online purchases
//                 reach.
//
// Degradation is deliberate and matches the session's own fallback: no
// catalogue (unit callers), no eligible merchant, or an unknown home
// area falls back to the caller's pool / the national weighting rather
// than inventing geography.
[[nodiscard]] entity::Key
pickMerchantDestination(random::Rng &rng, const IllicitContext &ctx,
                        entity::geography::GeoAreaId homeArea,
                        bool cardPresent, entity::Key fallback) {
  namespace geography = ::PhantomLedger::entity::geography;
  namespace commerce = ::PhantomLedger::activity::spending::market::commerce;

  if (ctx.merchants == nullptr || ctx.merchants->records.empty()) {
    return fallback;
  }

  const auto &records = ctx.merchants->records;
  const auto &geo = ::PhantomLedger::synth::geo::geography();

  const bool localAnchor = cardPresent && geo.contains(homeArea);
  const double scaleMiles =
      localAnchor ? commerce::cardPresentDecayScaleMiles(geo.at(homeArea))
                  : 0.0;

  std::vector<double> weights;
  std::vector<std::size_t> candidates;
  weights.reserve(records.size());
  candidates.reserve(records.size());

  for (std::size_t i = 0; i < records.size(); ++i) {
    const auto &rec = records[i];
    const bool online =
        rec.footprint == ::PhantomLedger::entity::merchant::Footprint::online;

    if (cardPresent) {
      if (online || !geo.contains(rec.location)) {
        continue;
      }
      double w = rec.weight;
      if (localAnchor && scaleMiles > 0.0) {
        const double miles =
            geography::distanceMiles(geo.at(homeArea), geo.at(rec.location));
        w *= std::exp(-miles / scaleMiles);
      }
      if (w > 0.0) {
        weights.push_back(w);
        candidates.push_back(i);
      }
    } else {
      if (!online || !(rec.weight > 0.0)) {
        continue;
      }
      weights.push_back(rec.weight);
      candidates.push_back(i);
    }
  }

  if (weights.empty()) {
    return fallback;
  }

  const auto cdf = probability::distributions::buildCdf(
      std::span<const double>(weights.data(), weights.size()));
  const auto pick =
      probability::distributions::sampleIndex(cdf, rng.nextDouble());

  return records[candidates[pick]].counterpartyId;
}

} // namespace

std::vector<transactions::Transaction>
generate(IllicitContext &ctx, std::span<const CompromisePlan> plans,
         std::int32_t budget) {
  std::vector<transactions::Transaction> out;
  if (budget <= 0 || plans.empty()) {
    return out;
  }
  out.reserve(static_cast<std::size_t>(budget) +
              static_cast<std::size_t>(budget) / 2);

  if (ctx.execution.factory == nullptr) {
    throw std::logic_error("unauthorized::generate requires the S9 "
                           "content-keyed RngFactory on Execution");
  }
  const auto &keyFactory = *ctx.execution.factory;
  std::array<char, 16> seqBuf{};

  // Reimbursement rows are flag-0 remediation (like camouflage rows),
  // so the fraud BUDGET bounds only flag-1 rows — never the
  // chargeback credits that follow a reported compromise.
  std::int32_t fraudEmitted = 0;

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
    const auto planTxf = ctx.execution.txf.rebound(rng);

    // card-fraud-realism-v2 step b: merchant selection draws on its OWN
    // named lane, so adding it cannot perturb the event/amount stream
    // above (or any ring lane). One modality decision and one merchant
    // draw per emitted card-rail row.
    auto merchantRng = keyFactory.rng(
        {"fraud", "unauth", "merchant", renderUInt(seqBuf, plan.seq)});

    // MODALITY, one decision per compromise case (a case has a modus
    // operandi). Gift-card scams are card-PRESENT by construction — the
    // coached victim walks into a store and buys the cards.
    const bool cardPresent =
        plan.rail == Rail::giftCardScam
            ? true
            : !merchantRng.coin(kCardNotPresentShare);

    events.clear();
    isTest.assign(static_cast<std::size_t>(target), false);

    // Card compromises are mostly noticed and disputed (Reg Z /
    // zero-liability: statement review catches the spends) — one
    // per-case decision, drawn before the event draws so the event
    // stream is unchanged for unreported cases. Gift-card scams and
    // ATO drains produce no reimbursement here (scams: recovery is
    // rare once codes are read out; ATO Reg E remediation is a
    // documented gap in docs/fraud_model_audit.md F-4).
    bool reported = false;

    switch (plan.rail) {
    case Rail::card: {
      // Phase boundaries (all offsets relative to plan.startTs).
      const std::int64_t tWindow =
          std::clamp<std::int64_t>(span / 6, 600, 3600);
      const std::int64_t spendStart =
          std::min(tWindow + std::max<std::int64_t>(3600, span / 6), span - 1);

      // Test-candidate decisions first (slot order), then the report
      // decision, then event draws.
      for (std::int32_t e = 0; e < target && e < 2; ++e) {
        isTest[static_cast<std::size_t>(e)] = rng.coin(0.7);
      }
      reported = rng.coin(0.85);
      for (std::int32_t e = 0; e < target; ++e) {
        Event ev{};
        ev.test = isTest[static_cast<std::size_t>(e)];
        if (ev.test) {
          ev.ts = plan.startTs + offsetIn(rng, 0, tWindow);
          ev.amount = amounts::cardTestCharge(rng);
        } else {
          ev.ts = plan.startTs + offsetIn(rng, spendStart, span);
          ev.amount = amounts::cardFraudSpend(rng, eventPriceScale(ev.ts));
        }
        events.push_back(ev);
      }
      break;
    }
    case Rail::ato: {
      for (std::int32_t e = 0; e < target; ++e) {
        Event ev{};
        ev.ts = plan.startTs + offsetIn(rng, 0, span);
        ev.amount = amounts::atoDrainAmount(rng, eventPriceScale(ev.ts));
        events.push_back(ev);
      }
      break;
    }
    case Rail::giftCardScam: {
      // One coached burst: the scammer keeps the victim on the phone,
      // cards bought minutes-to-hours apart at retail.
      for (std::int32_t e = 0; e < target; ++e) {
        Event ev{};
        ev.ts = plan.startTs + offsetIn(rng, 0, span);
        ev.amount = amounts::giftCardScamAmount(rng);
        events.push_back(ev);
      }
      break;
    }
    }

    std::stable_sort(
        events.begin(), events.end(),
        [](const Event &a, const Event &b) { return a.ts < b.ts; });

    for (const Event &ev : events) {
      if (fraudEmitted >= budget) {
        return out;
      }

      transactions::Draft draft{};
      draft.source = plan.victimAccount;
      draft.timestamp = ev.ts;
      draft.amount = ev.amount;
      draft.isFraud = 1;
      draft.ringId = -1;

      switch (plan.rail) {
      case Rail::card:
      case Rail::giftCardScam:
        // THE P0 FIX: the acceptance population, selected on the same
        // geographic axis legitimate card activity uses. The biller
        // pool survives only as the degraded fallback for callers that
        // supply no catalogue.
        draft.destination = pickMerchantDestination(
            merchantRng, ctx, plan.homeArea, cardPresent,
            pickOne(rng, ctx.billerAccounts));
        draft.channel = channels::tag(channels::Legit::cardPurchase);
        break;
      case Rail::ato:
        draft.destination = plan.dropAccount;
        draft.channel = channels::tag(channels::Legit::p2p);
        break;
      }

      out.push_back(planTxf.make(draft));
      ++fraudEmitted;
      out.back().session.deviceId = plan.device;
      out.back().session.ipAddress = plan.ip;
      out.back().fraud.type = plan.rail == Rail::giftCardScam
                                  ? ::PhantomLedger::fraud::FraudType::scamGiftCard
                                  : ::PhantomLedger::fraud::FraudType::txnFraudSolo;

      // Reported card compromise: each fraudulent SPEND (not the
      // sub-$5 test charges, which typically go unnoticed) is made
      // whole by a merchant chargeback credit 1-10 days later —
      // flag-0, typed none, no attacker session, outside the fraud
      // budget.
      if (plan.rail == Rail::card && reported && !ev.test) {
        const auto lag = offsetIn(rng, kSecondsPerDay, 10 * kSecondsPerDay);

        transactions::Draft credit{};
        credit.source = draft.destination;
        credit.destination = plan.victimAccount;
        credit.timestamp = ev.ts + lag;
        credit.amount = ev.amount;
        credit.isFraud = 0;
        credit.ringId = -1;
        credit.channel = channels::tag(channels::Credit::chargeback);

        out.push_back(planTxf.make(credit));
      }
    }
  }
  return out;
}

} // namespace PhantomLedger::transfers::fraud::typologies::unauthorized
