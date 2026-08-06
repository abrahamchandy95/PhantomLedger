#include "phantomledger/activity/spending/routing/router.hpp"

#include "phantomledger/activity/spending/market/commerce/affinity.hpp"
#include "phantomledger/activity/spending/market/commerce/local_pools.hpp"
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

/* Share of NON-FAVORITE merchant picks that are CARD-PRESENT (in-person,
 * LOCAL to the customer's home area) rather than online (geography-free).
 * PROVISIONAL.
 *
 * THIS CONSTANT SHAPES AN EXPORTED COLUMN. The card-fraud exporter's
 * `use_chip` does not assert an independent hash share; it READS the
 * realized outcome of this selection through the destination's footprint.
 *
 * REGISTERED DEBT: it should be TIME-VARYING across the modeled decades, as
 * e-commerce grows. `commerce::cnpShareForYear` already carries the dated,
 * cited series, but it governs favourite MEMBERSHIP only — this explore
 * branch is still flat. */
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

  /* `event.priceScale` (the day's CPI level, class P) turns the
   * calibration-year draw into era-correct nominal dollars — AFTER the draw,
   * BEFORE floorAndRound (the $1 floor is de-minimis and fixed).
   * `event.consumptionScale` applies the retirement step on the same seam. */
  const double raw = math::amounts::kBill.sample(rng_) * event.amountFactor *
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

  const double raw = math::amounts::kExternalUnknown.sample(rng_) *
                     event.amountFactor * event.priceScale *
                     event.consumptionScale;
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
    /* ZIPF, NOT UNIFORM. A `choiceIndex` over the row is an exponent of
     * zero, and this branch carries ~99.2% of all merchant picks. See
     * commerce/affinity.hpp for the citation, the self-checking arithmetic,
     * and why the rank is a hash of (person, merchant) rather than the slot
     * index.
     *
     * MUST STAY AT ONE UNIFORM: `sampleFavoriteSlot` consumes the draw
     * handed to it and derives the weights draw-free.
     *
     * Fully qualified because the local `commerce` binding above shadows the
     * namespace of the same name. */
    const auto slot = market::commerce::sampleFavoriteSlot(
        favRow, spender.personIndex, rng_.nextDouble());
    return favRow[slot];
  }

  /* Non-favourite (popularity/geography) pick: roll the transaction MODE.
   * Card-present everyday spend is LOCAL to the customer's home area and
   * samples the home-area distance-decay pool; online spend is
   * geography-free and samples the national popularity CDF. A home area with
   * no pool falls back to the national CDF. Favourites (returned above) do
   * not roll a mode.
   *
   * SELECTION READS ONLY GEOGRAPHY AND POPULARITY, never
   * isFraud/ringId/label. */
  const auto &geoPools = commerce.geoPools();
  const bool cardPresent = rng_.coin(kCardPresentShare);
  /* THE HOME MUST COME FROM THE POPULATION VIEW, which the monthly evolver
   * re-points from the relocation schedule — never from a cached `Spender`
   * copy. `prepareSpenders` runs ONCE for the whole fold and `runDay` takes
   * `PreparedRun` by const ref, so a cached home would freeze at the
   * window-start value while the exporter published the moves: the corpus
   * would disagree with itself and look correct doing it. */
  const auto homeArea = market_.population().homeArea(spender.person);
  const bool local = cardPresent && geoPools.has(homeArea);

  const auto drawPick = [&]() -> std::uint32_t {
    if (local) {
      return geoPools.sample(homeArea, rng_.nextDouble());
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
