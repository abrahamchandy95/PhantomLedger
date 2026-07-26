#include "phantomledger/activity/spending/routing/router.hpp"

#include "phantomledger/math/amounts.hpp"
#include "phantomledger/primitives/random/distributions/cdf.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"

#include <algorithm>
#include <cstdint>

namespace PhantomLedger::activity::spending::routing {

namespace {

inline constexpr channels::Tag kBillChannel =
    channels::tag(channels::Legit::bill);
inline constexpr channels::Tag kExternalChannel =
    channels::tag(channels::Legit::externalUnknown);
inline constexpr channels::Tag kP2pChannel =
    channels::tag(channels::Legit::p2p);
inline constexpr channels::Tag kMerchantChannel =
    channels::tag(channels::Legit::merchant);
inline constexpr channels::Tag kCardChannel =
    channels::tag(channels::Legit::cardPurchase);

inline constexpr double kCashCushionMultiple = 1.5;
inline constexpr double kAmountFloor = 1.0;

// geo-causal-v1 (G2a step-2): share of NON-FAVORITE merchant picks that are
// CARD-PRESENT (in-person, LOCAL to the customer's home area) rather than
// online (geography-free). PROVISIONAL — aligns with the card-fraud exporter's
// use_chip online share (~.11); it SHOULD become TIME-VARYING across the
// modeled decades (e-commerce grows) — see the CARD-PRESENT/ONLINE SHARE debt
// in systemprompt.md.
inline constexpr double kCardPresentShare = 0.89;

struct PaymentRoute {
  entity::Key srcKey{};
  clearing::Ledger::Index srcIdx = clearing::Ledger::invalid;
  channels::Tag channel{};
};

[[nodiscard]] PaymentRoute selectPaymentRoute(random::Rng &rng,
                                              const actors::Spender &spender,
                                              double cashAvailable,
                                              double cardAvailable,
                                              double txnAmount) {
  if (!spender.hasCard) {
    return {spender.depositAccount, spender.depositAccountIdx,
            kMerchantChannel};
  }

  const bool cardHasRoom = cardAvailable >= txnAmount;

  if (rng.coin(spender.cardShare)) {
    if (cardHasRoom) {
      return {spender.card, spender.cardIdx, kCardChannel};
    }
    return {spender.depositAccount, spender.depositAccountIdx,
            kMerchantChannel};
  }

  const double cushion = txnAmount * kCashCushionMultiple;
  if (cashAvailable < cushion && cardHasRoom) {
    return {spender.card, spender.cardIdx, kCardChannel};
  }

  return {spender.depositAccount, spender.depositAccountIdx, kMerchantChannel};
}

} // namespace

EmissionResult PaymentRouter::emitBill(const actors::Event &event) {
  const auto &commerce = market_.commerce();
  const auto billerRow = commerce.billers().rowOf(event.spender->personIndex);

  std::uint32_t billerIdx = 0;
  if (!billerRow.empty() && rng_.coin(policy_.preferKnownBillersP)) {
    const auto slot = rng_.choiceIndex(billerRow.size());
    billerIdx = billerRow[slot];
  } else {
    const auto &cdf = commerce.billerCdf();
    billerIdx = static_cast<std::uint32_t>(
        probability::distributions::sampleIndex(cdf, rng_.nextDouble()));
  }

  const auto *catalog = commerce.catalog();
  const entity::Key dst = catalog->records[billerIdx].counterpartyId;

  const auto dstIdx = billerIdx < resolved_.merchantCounterpartyIdx.size()
                          ? resolved_.merchantCounterpartyIdx[billerIdx]
                          : clearing::Ledger::invalid;

  // H1 step 2b (class P): event.priceScale (the day's CPI level) turns
  // the calibration-year draw into era-correct nominal dollars — after
  // the draw, before floorAndRound ($1 floor = de-minimis, fixed).
  // H2 step 2c: event.consumptionScale applies the retirement step on
  // the same seam.
  const double raw =
      math::amounts::kBill.sample(rng_) * event.amountFactor *
      event.priceScale * event.consumptionScale;
  const double amount = primitives::utils::floorAndRound(raw, kAmountFloor);

  EmissionResult result;
  result.draft = transactions::Draft{
      .source = event.spender->depositAccount,
      .destination = dst,
      .amount = amount,
      .timestamp = time::toEpochSeconds(event.ts),
      .isFraud = 0,
      .ringId = -1,
      .channel = kBillChannel,
  };
  result.srcIdx = event.spender->depositAccountIdx;
  result.dstIdx = dstIdx;
  return result;
}

EmissionResult PaymentRouter::emitExternal(const actors::Event &event) {
  const entity::Key dst =
      entity::makeKey(entity::Role::merchant, entity::Bank::external, 1u);

  const double raw =
      math::amounts::kExternalUnknown.sample(rng_) * event.amountFactor *
      event.priceScale * event.consumptionScale;
  const double amount = primitives::utils::floorAndRound(raw, kAmountFloor);

  EmissionResult result;
  result.draft = transactions::Draft{
      .source = event.spender->depositAccount,
      .destination = dst,
      .amount = amount,
      .timestamp = time::toEpochSeconds(event.ts),
      .isFraud = 0,
      .ringId = -1,
      .channel = kExternalChannel,
  };
  result.srcIdx = event.spender->depositAccountIdx;
  result.dstIdx = resolved_.externalUnknownIdx;
  return result;
}

std::optional<EmissionResult>
PaymentRouter::emitP2p(const actors::Event &event) {
  const auto &commerce = market_.commerce();
  const auto contactRow = commerce.contacts().rowOf(event.spender->personIndex);

  if (contactRow.empty()) {
    return emitExternal(event);
  }

  const auto slot = rng_.choiceIndex(contactRow.size());
  const auto contactPersonIndex = contactRow[slot];

  if (contactPersonIndex >= market_.population().count()) {
    return emitExternal(event);
  }

  const auto contactPerson =
      static_cast<entity::PersonId>(contactPersonIndex + 1);
  const auto dst = market_.population().primary(contactPerson);

  if (!entity::valid(dst) || dst == event.spender->depositAccount) {
    return emitExternal(event);
  }

  const double raw = math::amounts::kP2P.sample(rng_) *
                     event.spender->amountMultiplier * event.amountFactor *
                     event.priceScale * event.consumptionScale;
  const double amount = primitives::utils::floorAndRound(raw, kAmountFloor);

  const auto dstIdx = contactPersonIndex < resolved_.personPrimaryIdx.size()
                          ? resolved_.personPrimaryIdx[contactPersonIndex]
                          : clearing::Ledger::invalid;

  EmissionResult result;
  result.draft = transactions::Draft{
      .source = event.spender->depositAccount,
      .destination = dst,
      .amount = amount,
      .timestamp = time::toEpochSeconds(event.ts),
      .isFraud = 0,
      .ringId = -1,
      .channel = kP2pChannel,
  };
  result.srcIdx = event.spender->depositAccountIdx;
  result.dstIdx = dstIdx;
  return result;
}

std::uint32_t PaymentRouter::pickMerchantIndex(const actors::Spender &spender,
                                               double exploreP) {
  const auto &commerce = market_.commerce();
  const auto favRow = commerce.favorites().rowOf(spender.personIndex);
  const bool exploring = rng_.coin(exploreP);

  if (!exploring && !favRow.empty()) {
    const auto slot = rng_.choiceIndex(favRow.size());
    return favRow[slot];
  }

  // Non-favorite (popularity/geography) pick. geo-causal-v1 (G2a step-2): roll
  // the transaction MODE. Card-present everyday spend is LOCAL to the
  // customer's home area — sample the home-area distance-decay pool; online
  // spend is geography-free — sample the national popularity CDF. When the
  // home area has no pool (no local anchor), card-present falls back to the
  // national CDF. Favorites (returned above) stay global-random regardless of
  // mode (axiom #8). The mode roll is a NEW rng draw — it is what shifts the
  // corpus in this round. Selection reads ONLY geography/popularity, never
  // isFraud/ringId/label.
  const auto &geoPools = commerce.geoPools();
  const bool cardPresent = rng_.coin(kCardPresentShare);
  const bool local = cardPresent && geoPools.has(spender.homeArea);

  const auto drawPick = [&]() -> std::uint32_t {
    if (local) {
      return geoPools.sample(spender.homeArea, rng_.nextDouble());
    }
    return static_cast<std::uint32_t>(probability::distributions::sampleIndex(
        commerce.merchCdf(), rng_.nextDouble()));
  };

  std::uint32_t pick = drawPick();

  if (favRow.empty()) {
    return pick;
  }

  for (std::uint16_t attempt = 0; attempt < policy_.merchantRetryLimit;
       ++attempt) {
    const bool isFav =
        std::find(favRow.begin(), favRow.end(), pick) != favRow.end();
    if (!isFav) {
      return pick;
    }
    pick = drawPick();
  }
  return pick;
}

std::optional<EmissionResult>
PaymentRouter::emitMerchant(const actors::Event &event) {
  const auto &commerce = market_.commerce();
  const auto *catalog = commerce.catalog();
  if (catalog == nullptr) {
    return std::nullopt;
  }

  const std::uint32_t merchantIdx =
      pickMerchantIndex(*event.spender, event.exploreP);

  const auto &record = catalog->records[merchantIdx];
  const auto dst = record.counterpartyId;
  const auto category = record.category;

  const double rawAmount = math::amounts::merchantAmount(rng_, category) *
                           event.spender->amountMultiplier *
                           event.amountFactor * event.priceScale *
                           event.consumptionScale;
  const double amount =
      primitives::utils::floorAndRound(rawAmount, kAmountFloor);

  const auto route = selectPaymentRoute(
      rng_, *event.spender, event.availableCash, event.cardAvailable, amount);

  const auto dstIdx = merchantIdx < resolved_.merchantCounterpartyIdx.size()
                          ? resolved_.merchantCounterpartyIdx[merchantIdx]
                          : clearing::Ledger::invalid;

  EmissionResult result;
  result.draft = transactions::Draft{
      .source = route.srcKey,
      .destination = dst,
      .amount = amount,
      .timestamp = time::toEpochSeconds(event.ts),
      .isFraud = 0,
      .ringId = -1,
      .channel = route.channel,
  };
  result.srcIdx = route.srcIdx;
  result.dstIdx = dstIdx;
  return result;
}

std::optional<EmissionResult> PaymentRouter::route(Slot slot,
                                                   const actors::Event &event) {
  switch (slot) {
  case Slot::merchant:
    return emitMerchant(event);
  case Slot::bill:
    return emitBill(event);
  case Slot::p2p:
    return emitP2p(event);
  case Slot::externalUnknown:
    return emitExternal(event);
  case Slot::kCount:
    break;
  }
  return std::nullopt;
}

} // namespace PhantomLedger::activity::spending::routing
