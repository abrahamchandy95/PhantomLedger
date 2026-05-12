#include "phantomledger/activity/spending/routing/router.hpp"

#include "phantomledger/math/amounts.hpp"
#include "phantomledger/primitives/random/distributions/cdf.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"

#include <algorithm>
#include <cstdint>

namespace PhantomLedger::spending::routing {

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

struct PaymentRoute {
  entity::Key srcKey{};
  clearing::Ledger::Index srcIdx = clearing::Ledger::invalid;
  channels::Tag channel{};
};

[[nodiscard]] PaymentRoute selectPaymentRoute(random::Rng &rng,
                                              const actors::Spender &spender,
                                              double cashAvailable,
                                              double txnAmount) {
  if (!spender.hasCard) {
    return {spender.depositAccount, spender.depositAccountIdx,
            kMerchantChannel};
  }

  if (rng.coin(spender.cardShare)) {
    return {spender.card, spender.cardIdx, kCardChannel};
  }

  const double cushion = txnAmount * kCashCushionMultiple;
  if (cashAvailable < cushion) {
    return {spender.card, spender.cardIdx, kCardChannel};
  }

  return {spender.depositAccount, spender.depositAccountIdx, kMerchantChannel};
}

} // namespace

// ----------------------------- Bill --------------------------------

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

  const double amount = math::amounts::kBill.sample(rng_);

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

// --------------------------- External ------------------------------

EmissionResult PaymentRouter::emitExternal(const actors::Event &event) {
  const entity::Key dst =
      entity::makeKey(entity::Role::merchant, entity::Bank::external, 1u);

  const double amount = primitives::utils::floorAndRound(
      math::amounts::kExternalUnknown.sample(rng_),
      /*floor=*/1.0);

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

// ----------------------------- P2P ---------------------------------

std::optional<EmissionResult>
PaymentRouter::emitP2p(const actors::Event &event) {
  const auto &commerce = market_.commerce();
  const auto contactRow = commerce.contacts().rowOf(event.spender->personIndex);

  if (contactRow.empty()) {
    return std::nullopt;
  }

  const auto slot = rng_.choiceIndex(contactRow.size());
  const auto contactPersonIndex = contactRow[slot];

  if (contactPersonIndex >= market_.population().count()) {
    return std::nullopt;
  }

  const auto contactPerson =
      static_cast<entity::PersonId>(contactPersonIndex + 1);
  const auto dst = market_.population().primary(contactPerson);

  if (!entity::valid(dst) || dst == event.spender->depositAccount) {
    return std::nullopt;
  }

  const double raw =
      math::amounts::kP2P.sample(rng_) * event.spender->amountMultiplier;
  const double amount = primitives::utils::floorAndRound(raw, /*floor=*/1.0);

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

// --------------------------- Merchant ------------------------------

std::uint32_t PaymentRouter::pickMerchantIndex(const actors::Spender &spender,
                                               double exploreP) {
  const auto &commerce = market_.commerce();
  const auto favRow = commerce.favorites().rowOf(spender.personIndex);
  const bool exploring = rng_.coin(exploreP);

  if (!exploring && !favRow.empty()) {
    const auto slot = rng_.choiceIndex(favRow.size());
    return favRow[slot];
  }

  const auto &cdf = commerce.merchCdf();
  std::uint32_t pick = static_cast<std::uint32_t>(
      probability::distributions::sampleIndex(cdf, rng_.nextDouble()));

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
    pick = static_cast<std::uint32_t>(
        probability::distributions::sampleIndex(cdf, rng_.nextDouble()));
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
                           event.spender->amountMultiplier;
  const double amount =
      primitives::utils::floorAndRound(rawAmount, /*floor=*/1.0);

  // cc_share + liquidity-aware fallback. See selectPaymentRoute above.
  const auto route =
      selectPaymentRoute(rng_, *event.spender, event.availableCash, amount);

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

// --------------------------- Dispatch ------------------------------

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

} // namespace PhantomLedger::spending::routing
