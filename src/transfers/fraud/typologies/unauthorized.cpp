#include "phantomledger/transfers/fraud/typologies/unauthorized.hpp"

#include "phantomledger/activity/spending/market/commerce/local_pools.hpp"
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
#include <limits>
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

// ============ THE COACHED GIFT-CARD PURCHASE IS NOT ALWAYS IN A STORE
// (giftcard-channel-2026-07)
//
// This rail was hardcoded card-PRESENT, with the comment "the coached
// victim walks into a store and buys the cards". That describes the
// classic pattern and it was 100% of the corpus — which is wrong in a way
// that matters for exactly the detection problem this dataset exists to
// support.
//
// A coached victim is also routinely walked through buying a DIGITAL gift
// card: an Apple / Google Play / Amazon e-gift code, purchased online and
// delivered by email, whose number is then read out over the phone. In
// that case the purchase is card-NOT-present, the destination is an online
// acceptance endpoint, and **the session address on the row is genuinely
// the victim's own home IP** — because the victim really is sitting at
// their own machine, under instruction, doing it themselves.
//
// That distinction is the whole point of the rail. The intervention that
// helps a scam victim is TIME TO RECONSIDER, and the online purchase is
// the branch where an issuer actually has a session to score in real time
// and something to interrupt. Collapsing every case to an in-store swipe
// deleted the branch where the model can act.
//
// DIGITAL SHARE, IN BASIS POINTS, BY CALENDAR YEAR. CLASS S, CALIBRATION
// UNCITED — mirrors `derive::chipShareBasisPoints` in construction and in
// honesty. The DIRECTION and the ordering are the anchored claims:
// consumer e-gift codes barely existed in the early corpus era, retailer
// e-gift programmes and the digital-code marketplaces grew through the
// 2010s, and the in-store physical rack REMAINS THE MAJORITY throughout
// the window — the FTC-documented pattern of the period is a victim sent
// to a drugstore or big-box store. The anchor numbers are a declared
// modelling choice, not transcribed from a named series; promote to CITED
// and re-pin in whatever arc wires a gift-card-channel-mix source.
[[nodiscard]] constexpr std::uint32_t
digitalGiftCardShareBasisPoints(std::int32_t year) noexcept {
  constexpr std::int32_t kFirstDigitalYear = 2005;
  constexpr std::array<std::uint16_t, 19> kDigitalShareBp{
      100,   // 2005 e-gift codes appear, essentially a novelty
      200,   // 2006
      400,   // 2007
      600,   // 2008
      900,   // 2009
      1'200, // 2010
      1'500, // 2011
      1'800, // 2012 app-store codes become a scam staple
      2'100, // 2013
      2'400, // 2014
      2'600, // 2015
      2'800, // 2016
      3'000, // 2017
      3'300, // 2018
      3'500, // 2019
      3'800, // 2020
      4'100, // 2021
      4'300, // 2022
      4'500, // 2023 — frozen outside coverage, still a minority
  };
  if (year < kFirstDigitalYear) {
    return 0U;
  }
  const auto offset = std::min<std::int64_t>(
      year - kFirstDigitalYear,
      static_cast<std::int64_t>(kDigitalShareBp.size()) - 1);
  return kDigitalShareBp[static_cast<std::size_t>(offset)];
}

[[nodiscard]] inline double digitalGiftCardShare(std::int64_t epochSec) {
  const auto year = ::PhantomLedger::time::toCalendarDate(
                        ::PhantomLedger::time::fromEpochSeconds(epochSec))
                        .year;
  return static_cast<double>(digitalGiftCardShareBasisPoints(year)) / 10'000.0;
}

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
//                 decayScaleMilesFor(home)) — so a stolen card
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
// venue-reuse-2026-08 —————————————————————————————————————————————————
//
// SLOTS PER CASE. A compromise spends at a SMALL SET of venues, not one and
// not a fresh draw per row. Three is measured, not chosen: collapsing a case
// onto ONE merchant is the worst option on both axes at once, because a
// one-merchant case can no longer collide with another case at all — the
// probe measured cross-victim overlap COLLAPSING from 0.3333 to 0.0215 while
// P(pure-fraud share >= 0.10) rose from 0.000 to 0.078. The intuitive fix is
// the wrong one.
inline constexpr std::size_t kMerchantSlots = 3;

// The share of online weight mass the campaign-shared cash-out venue is drawn
// from. See `campaignVenue` for why this is a cliff rather than a dial.
inline constexpr double kCashOutHeadMass = 0.25;

// A distinct hash domain, so the cash-out venue shares no bits with any other
// draw-free predicate in the corpus (the two-domain argument in
// `merchant_ownership.hpp` applies verbatim).
inline constexpr std::uint64_t kVenueDomain = 0x56'454E'5545'0001ULL;

[[nodiscard]] constexpr std::uint64_t venueMix(std::uint64_t value) noexcept {
  value += 0x9E3779B97F4A7C15ULL;
  value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
  return value ^ (value >> 31U);
}

// A stable uniform in [0,1) for a campaign index.
[[nodiscard]] constexpr double campaignUnit(std::uint32_t campaign) noexcept {
  const auto mixed = venueMix(kVenueDomain ^ venueMix(campaign));
  return static_cast<double>(venueMix(mixed) >> 11U) * 0x1.0p-53;
}

// venue-reuse-2026-08: the eligible acceptance population for ONE case,
// with its selection weights. Everything this depends on — the victim's home
// area, the modality coin, the case timestamp — is fixed for the whole case,
// so the pool is built ONCE PER PLAN. It used to be rebuilt inside the event
// loop, an O(catalogue) scan per fraud row.
struct MerchantPool {
  std::vector<double> weights;
  std::vector<std::size_t> candidates;

  [[nodiscard]] bool empty() const noexcept { return weights.empty(); }
};

[[nodiscard]] MerchantPool
buildMerchantPool(const IllicitContext &ctx,
                  entity::geography::GeoAreaId homeArea, bool cardPresent,
                  std::int64_t ts) {
  namespace geography = ::PhantomLedger::entity::geography;
  namespace commerce = ::PhantomLedger::activity::spending::market::commerce;

  MerchantPool pool;
  if (ctx.merchants == nullptr || ctx.merchants->records.empty()) {
    return pool;
  }

  const auto &records = ctx.merchants->records;
  const auto &geo = ::PhantomLedger::synth::geo::geography();

  const bool localAnchor = cardPresent && geo.contains(homeArea);
  const double scaleMiles =
      localAnchor ? commerce::decayScaleMilesFor(geo.at(homeArea))
                  : 0.0;

  auto &weights = pool.weights;
  auto &candidates = pool.candidates;
  weights.reserve(records.size());
  candidates.reserve(records.size());

  for (std::size_t i = 0; i < records.size(); ++i) {
    const auto &rec = records[i];
    // merchant-churn-2026-07: fraud cannot land on a merchant that had not
    // opened or had already closed. This filter uses the CASE'S OWN
    // timestamp, which is strictly stronger than the legitimate path's
    // month-boundary rebuild — the candidate list is rebuilt per case here
    // anyway, so point-in-time exactness is free.
    //
    // DRAW-COUNT NEUTRAL: the candidate list shrinks but the number of
    // uniforms spent stays exactly one, so this cannot shift any other
    // plan's draws.
    if (!rec.liveAt(ts)) {
      continue;
    }
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

  return pool;
}

// One weighted pick from a built pool, spending exactly one caller-supplied
// uniform. Identical distribution to the pre-round per-row draw.
[[nodiscard]] entity::Key pickFromPool(const entity::merchant::Catalog &cat,
                                       const MerchantPool &pool, double u) {
  const auto cdf = probability::distributions::buildCdf(
      std::span<const double>(pool.weights.data(), pool.weights.size()));
  const auto pick = probability::distributions::sampleIndex(cdf, u);
  return cat.records[pool.candidates[pick]].counterpartyId;
}

// venue-reuse-2026-08: the CAMPAIGN'S SHARED CASH-OUT VENUE.
//
// DRAW-FREE — a splitmix64 of the campaign index becomes the position, so
// every case of one campaign converges on the same outlet without spending a
// uniform and without any cross-case state. That is what keeps the batch and
// windowed engines in lockstep: the answer depends on the campaign index and
// the live catalogue, never on how many cases ran before.
//
// HEAD-RESTRICTED, AND THE RESTRICTION IS LOAD-BEARING RATHER THAN COSMETIC.
// An unrestricted shared slot concentrates a campaign's whole case load (up to
// 22 cases at the pop-900 leg) onto one arbitrary outlet, which manufactures
// fraud-only merchants: the probe measured P(pure-fraud share >= 0.10) at
// 0.097 unrestricted against 0.000 at this q. Restricting to outlets that
// already carry heavy LEGITIMATE traffic is what keeps the venue's fraud rows
// diluted by real ones.
//
// q IS A CLIFF, NOT A DIAL. q = 0.50 measured WORSE than no restriction at all
// (0.147) because the online head at that mass contains outlets with zero
// legitimate rows — catalogue weight is only a partial proxy for realized
// traffic. Do not loosen q without re-measuring zero-legit head membership at
// every leg.
[[nodiscard]] entity::Key campaignVenue(const entity::merchant::Catalog &cat,
                                        const MerchantPool &pool,
                                        std::uint32_t campaign) {
  const auto n = pool.weights.size();

  // Rank by weight, descending; ties broken by catalogue index so the order is
  // content-determined rather than left to an unstable sort.
  std::vector<std::size_t> order(n);
  for (std::size_t i = 0; i < n; ++i) {
    order[i] = i;
  }
  std::sort(order.begin(), order.end(),
            [&](std::size_t a, std::size_t b) {
              if (pool.weights[a] != pool.weights[b]) {
                return pool.weights[a] > pool.weights[b];
              }
              return pool.candidates[a] < pool.candidates[b];
            });

  double total = 0.0;
  for (const double w : pool.weights) {
    total += w;
  }

  // Smallest prefix carrying at least kCashOutHeadMass of the weight. Always
  // non-empty: the first outlet alone qualifies when one dominates.
  const double target = total * kCashOutHeadMass;
  double headMass = 0.0;
  std::size_t headCount = 0;
  while (headCount < n && headMass < target) {
    headMass += pool.weights[order[headCount]];
    ++headCount;
  }

  // Position by cumulative weight INSIDE the head rather than by index, so a
  // churned-out outlet moves only the picks that landed on it — keying on the
  // index would re-point every campaign whenever head membership changed.
  const double u = campaignUnit(campaign);
  double acc = 0.0;
  const double want = u * headMass;
  for (std::size_t k = 0; k < headCount; ++k) {
    acc += pool.weights[order[k]];
    if (want < acc) {
      return cat.records[pool.candidates[order[k]]].counterpartyId;
    }
  }
  return cat.records[pool.candidates[order[headCount - 1]]].counterpartyId;
}

// The row's LABEL CLASS follows the rail. The two authorized rails are
// separate labels because the payment MECHANISM is what a model sees:
// a gift-card burst is a denomination lattice at a retail merchant, an
// impostor push is a continuous amount to a payee account.
[[nodiscard]] constexpr ::PhantomLedger::fraud::FraudType
fraudTypeFor(Rail rail) noexcept {
  switch (rail) {
  case Rail::giftCardScam:
    return ::PhantomLedger::fraud::FraudType::scamGiftCard;
  case Rail::scamImpostor:
    return ::PhantomLedger::fraud::FraudType::scamImpostor;
  case Rail::card:
  case Rail::ato:
    break;
  }
  return ::PhantomLedger::fraud::FraudType::txnFraudSolo;
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
    if (plan.eventEndTsExclusive <= plan.startTs ||
        (plan.eventEndTsExclusive != std::numeric_limits<std::int64_t>::max() &&
         plan.eventEndTsExclusive - plan.startTs < span)) {
      // Production planning reselects these cases without spending budget.
      // Standalone callers get the same safe behavior: reject rather than
      // compressing a long case into an unrealistic boundary burst.
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
    // operandi).
    //
    // The gift-card rail used to be hardcoded `true` here. It now draws a
    // DATED digital share: the coached victim is sent to a physical rack
    // most of the time, and increasingly over the corpus era is instead
    // walked through buying an e-gift code online — in which case the row
    // is card-not-present, the destination is an online acceptance
    // endpoint, and the session IP on the row is the victim's own, because
    // it really is the victim at their own machine. See
    // `digitalGiftCardShareBasisPoints` for why that branch has to exist.
    //
    // THE GIFT-CARD BRANCH DOES NOT ALWAYS SPEND A COIN, and a previous
    // version of this comment claimed it did. `Rng::coin` short-circuits at
    // p <= 0 (rng.hpp), and `digitalGiftCardShareBasisPoints` returns 0 before
    // 2005 — which every gate leg and the golden window start in. Harmless,
    // because the lane is keyed per plan and read by nothing else, but any
    // reasoning about fixed offsets on this lane is wrong.
    const bool cardPresent =
        plan.rail == Rail::giftCardScam
            ? !merchantRng.coin(digitalGiftCardShare(plan.startTs))
            : !merchantRng.coin(kCardNotPresentShare);

    // venue-reuse-2026-08: resolve this case's VENUE SLOTS once. The pool
    // depends only on per-case values (home area, modality, case timestamp),
    // so building it here rather than per row is both the fix and a large
    // saving — it was an O(catalogue) scan on every fraud row.
    const auto pool =
        buildMerchantPool(ctx, plan.homeArea, cardPresent, plan.startTs);
    std::array<entity::Key, kMerchantSlots> venues{};
    const bool haveVenues = !pool.empty();

    // ALL kMerchantSlots UNIFORMS ARE DRAWN UNCONDITIONALLY, including the one
    // slot 0 may discard below. That is deliberate: it makes this lane's draw
    // count independent of whether a campaign resolved, so no branch can shift
    // it. Same discipline as the planner's four-uniform block.
    for (std::size_t s = 0; s < kMerchantSlots; ++s) {
      const double u = merchantRng.nextDouble();
      if (haveVenues) {
        venues[s] = pickFromPool(*ctx.merchants, pool, u);
      }
    }

    // THE SHARED CASH-OUT VENUE IS CARD-NOT-PRESENT ONLY, and that is a
    // measured scoping decision rather than a simplification. The
    // card-present pool is decayed around each victim's OWN home area, and
    // campaign co-victims are selected by exposure, not co-location — so a
    // shared physical slot would contribute essentially ZERO cross-victim
    // overlap while still concentrating fraud onto the popularity head. It
    // would be pure cost. What ships is therefore an ONLINE cash-out
    // analogue; it must not be described as physical POS-breach CPP.
    if (haveVenues && !cardPresent &&
        plan.campaign != CompromisePlan::kNoCampaign) {
      venues[0] = campaignVenue(*ctx.merchants, pool, plan.campaign);
    }

    // Round-robin, matching the assignment the probe actually measured.
    std::size_t merchantRow = 0;

    events.clear();
    isTest.assign(static_cast<std::size_t>(target), false);

    // Card compromises are mostly noticed and disputed (Reg Z /
    // zero-liability: statement review catches the spends) — one
    // per-case decision, drawn before the event draws so the event
    // stream is unchanged for unreported cases. Gift-card scams and
    // ATO drains produce no reimbursement here (scams: recovery is
    // rare once codes are read out; ATO Reg E remediation is a
    // documented gap in docs/fraud_model_audit.md F-4).
    //
    // NO reimbursement exists for either AUTHORIZED rail, and that is a
    // modeled fact rather than an omission: in the corpus era a push the
    // victim authorized was their own instruction — US Reg E covers
    // unauthorized transfers, and the UK reimbursement code postdates
    // the window.
    bool reported = false;

    // victimization-v3: the impostor rail's payment method, one decision
    // per case (see the scamImpostor branch below).
    bool wireRail = false;

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
    case Rail::scamImpostor: {
      // The victim pushes their OWN money out under deception. One
      // per-case decision first — a wire-shaped external transfer or an
      // app push — drawn before the event loop so the amount stream does
      // not depend on it. BOTH channels carry heavy legitimate volume,
      // and that is the point: the rail must not label the row.
      wireRail = rng.coin(0.5);
      for (std::int32_t e = 0; e < target; ++e) {
        Event ev{};
        ev.ts = plan.startTs + offsetIn(rng, 0, span);
        // The victim's AGE-GRADED severity scales the whole
        // distribution, clamps included, so the tail shape stays
        // age-invariant and only its level moves.
        ev.amount =
            amounts::scamWireAmount(rng, eventPriceScale(ev.ts), plan.severity);
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
        //
        // `pickOne` MUST STAY EAGER AND IN-LOOP. It spends one choiceIndex on
        // the PLAN lane per emitted card row, and the chargeback lag draw
        // below reads that lane afterwards — making it lazy (evaluating it
        // only when the fallback is needed) is the single most tempting
        // simplification here and it would move every reimbursement timestamp
        // on every reported card case.
        {
          const auto billerFallback = pickOne(rng, ctx.billerAccounts);
          draft.destination =
              haveVenues ? venues[merchantRow % kMerchantSlots]
                         : billerFallback;
          ++merchantRow;
        }
        draft.channel = channels::tag(channels::Legit::cardPurchase);
        break;
      case Rail::ato:
        draft.destination = plan.dropAccount;
        draft.channel = channels::tag(channels::Legit::p2p);
        break;
      case Rail::scamImpostor:
        // An ordinary outbound push to the attacker's payee account, on
        // a rail the victim uses legitimately: a wire-shaped external
        // transfer or an app push.
        draft.destination = plan.dropAccount;
        draft.channel = wireRail
                            ? channels::tag(channels::Legit::externalUnknown)
                            : channels::tag(channels::Legit::p2p);
        break;
      }

      out.push_back(planTxf.make(draft));
      ++fraudEmitted;

      // WHO OPERATED THE ROW (victim-session-2026-07, roadmap step d;
      // closes the authorized-push half of the deferred finding).
      //
      // THE ATTACKER SESSION IS ONLY CORRECT ON THE UNAUTHORIZED RAILS.
      // On `card` and `ato` a third party is transacting with stolen
      // credentials, so the exogenous device/IP is the modeled truth. On
      // the two AUTHORIZED rails the operator is the VICTIM, on their
      // own device: the gift-card victim walks into a store and buys the
      // cards themselves, and the impostor-push victim wires or app-pushes
      // their own money. Stamping the attacker's session over those rows
      // asserted the opposite of the typology.
      //
      // THE VICTIM'S SESSION IS ALREADY ON THE ROW — no carrier needed.
      // `planTxf.make(draft)` above resolved it: `draft.ringId` is -1 so
      // the SharedInfra branch is skipped, all four rails carry a
      // customer-session channel (cardPurchase / p2p / externalUnknown —
      // none is payday-inbound and none is in `isExternallyInitiated`),
      // and `draft.source` is the VICTIM'S account, so
      // transactions::Factory routed `ownerOf(victim) -> routeDeviceFor /
      // routeIpFor` on this plan's own rng lane. The two lines below were
      // discarding that result. Skipping them on the authorized rails
      // consumes NO new randomness and advances NO new sticky state — the
      // routing had already happened either way.
      //
      // THIS SUPERSEDES THE RECORDED DEFERRAL RATIONALE, which held that
      // a victim-own-device fix would have to call `infra::Router` from
      // the fraud planner and would thereby advance that person's sticky
      // device index and perturb legitimate routing in later windows.
      // The advance is already paid, by the code above, in both engines
      // identically.
      //
      // AND DO NOT "IMPROVE" THIS BY PICKING THE POOL'S FIRST ENTRY.
      // A non-advancing `devicesByPerson[person].front()` read looks
      // safer but is strictly worse: it would hand every authorized-rail
      // row slot 0 while that same victim's LEGITIMATE rows follow the
      // sticky index, so "device != this person's current device" would
      // become the replacement shortcut. Letting the router's own result
      // stand is what makes the scam row's session indistinguishable, by
      // construction, from a legitimate row of the same victim.
      //
      // The former exported-device shortcut is also closed. Although
      // `plan.device` retains OwnerType::ring as internal routing state,
      // renderDeviceId maps every owner type into the same fixed-width,
      // role-neutral `D...` namespace. Card/ATO rows therefore preserve
      // the correct attacker session without exposing an `FD`/`LD`
      // prefix or a disjoint numeric range to the model.
      //
      // AN UNASSIGNED PLAN ENDPOINT IS ITSELF THE INSTRUCTION
      // (attacker-infra-2026-07). The planner now leaves `plan.device`
      // unassigned whenever the case was operated from the VICTIM'S own
      // endpoint — a banking trojan or remote-access tool driving the
      // victim's session, or household fraud on the victim's own machine
      // — and in that case the session `planTxf.make` already routed for
      // the victim is exactly right, by the same argument the two
      // authorized rails rest on.
      //
      // This deliberately does NOT get a new plan flag. The value was
      // already on the row: "no attacker endpoint" is fully expressed by
      // the absence of one, a standalone caller that supplies an
      // endpoint keeps its previous meaning bit-for-bit, and there is no
      // second field that a future call site could set inconsistently
      // with the first.
      if (!authorizedRail(plan.rail) && plan.device.assigned() &&
          plan.ip.value != 0) {
        out.back().session.deviceId = plan.device;
        out.back().session.ipAddress = plan.ip;
      }
      out.back().fraud.type = fraudTypeFor(plan.rail);

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

        if (credit.timestamp < plan.eventEndTsExclusive) {
          out.push_back(planTxf.make(credit));
        }
      }
    }
  }
  return out;
}

} // namespace PhantomLedger::transfers::fraud::typologies::unauthorized
