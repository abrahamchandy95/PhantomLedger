#include "phantomledger/spending/routing/payments.hpp"

#include "phantomledger/math/amounts.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/probability/distributions/cdf.hpp"
#include "phantomledger/spending/actors/spender.hpp"
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

[[nodiscard]] std::uint32_t
pickMerchantIndex(random::Rng &rng, const market::commerce::View &commerce,
                  const actors::Spender &spender, double exploreP,
                  std::uint16_t maxRetries) {
  const auto favRow = commerce.favorites().rowOf(spender.personIndex);
  const bool exploring = rng.coin(exploreP);

  if (!exploring && !favRow.empty()) {
    const auto slot = rng.choiceIndex(favRow.size());
    return favRow[slot];
  }

  // Explore branch: draw from global CDF and reject favorites a few times.
  const auto &cdf = commerce.merchCdf();
  std::uint32_t pick = static_cast<std::uint32_t>(
      distributions::sampleIndex(cdf, rng.nextDouble()));

  if (favRow.empty()) {
    return pick;
  }

  for (std::uint16_t attempt = 0; attempt < maxRetries; ++attempt) {
    const bool isFav =
        std::find(favRow.begin(), favRow.end(), pick) != favRow.end();
    if (!isFav) {
      return pick;
    }
    pick = static_cast<std::uint32_t>(
        distributions::sampleIndex(cdf, rng.nextDouble()));
  }
  return pick;
}

struct PaymentRoute {
  entity::Key srcKey{};
  clearing::Ledger::Index srcIdx = clearing::Ledger::invalid;
  channels::Tag channel{};
};

[[nodiscard]] PaymentRoute selectPaymentRoute(random::Rng &rng,
                                              const actors::Spender &spender) {
  if (!spender.hasCard) {
    return {spender.depositAccount, spender.depositAccountIdx,
            kMerchantChannel};
  }
  // Reads cached `cardShare` directly instead of `persona->card.share`.
  if (rng.coin(spender.cardShare)) {
    return {spender.card, spender.cardIdx, kCardChannel};
  }
  return {spender.depositAccount, spender.depositAccountIdx, kMerchantChannel};
}

} // namespace

// ----------------------------- Bill --------------------------------

EmissionResult emitBill(random::Rng &rng, const market::Market &market,
                        const PaymentRoutingRules &policy,
                        const ResolvedAccounts &resolved,
                        const actors::Event &event) {
  const auto &commerce = market.commerce();
  const auto billerRow = commerce.billers().rowOf(event.spender->personIndex);

  std::uint32_t billerIdx = 0;
  if (!billerRow.empty() && rng.coin(policy.preferKnownBillersP)) {
    const auto slot = rng.choiceIndex(billerRow.size());
    billerIdx = billerRow[slot];
  } else {
    const auto &cdf = commerce.billerCdf();
    billerIdx = static_cast<std::uint32_t>(
        distributions::sampleIndex(cdf, rng.nextDouble()));
  }

  const auto *catalog = commerce.catalog();
  const entity::Key dst = catalog->records[billerIdx].counterpartyId;

  // Pre-resolved destination index. Falls back to `invalid` (external)
  // when the table is empty (no ledger configured) or the slot is
  // out-of-range — the latter shouldn't happen in practice but the
  // bounds check is free.
  const auto dstIdx = billerIdx < resolved.merchantCounterpartyIdx.size()
                          ? resolved.merchantCounterpartyIdx[billerIdx]
                          : clearing::Ledger::invalid;

  const double amount = math::amounts::kBill.sample(rng);

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

EmissionResult emitExternal(random::Rng &rng, const market::Market &market,
                            const ResolvedAccounts &resolved,
                            const actors::Event &event) {
  (void)market;
  const entity::Key dst =
      entity::makeKey(entity::Role::merchant, entity::Bank::external, 1u);

  const double amount = primitives::utils::floorAndRound(
      math::amounts::kExternalUnknown.sample(rng),
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
  result.dstIdx = resolved.externalUnknownIdx;
  return result;
}

// ----------------------------- P2P ---------------------------------

std::optional<EmissionResult> emitP2p(random::Rng &rng,
                                      const market::Market &market,
                                      const ResolvedAccounts &resolved,
                                      const actors::Event &event) {
  const auto &commerce = market.commerce();
  const auto contactRow = commerce.contacts().rowOf(event.spender->personIndex);

  if (contactRow.empty()) {
    return std::nullopt;
  }

  const auto slot = rng.choiceIndex(contactRow.size());
  const auto contactPersonIndex = contactRow[slot];

  if (contactPersonIndex >= market.population().count()) {
    return std::nullopt;
  }

  const auto contactPerson =
      static_cast<entity::PersonId>(contactPersonIndex + 1);
  const auto dst = market.population().primary(contactPerson);

  if (!entity::valid(dst) || dst == event.spender->depositAccount) {
    return std::nullopt;
  }

  // Reads cached `amountMultiplier` directly.
  const double raw =
      math::amounts::kP2P.sample(rng) * event.spender->amountMultiplier;
  const double amount = primitives::utils::floorAndRound(raw, /*floor=*/1.0);

  // Pre-resolved destination index for the recipient.
  const auto dstIdx = contactPersonIndex < resolved.personPrimaryIdx.size()
                          ? resolved.personPrimaryIdx[contactPersonIndex]
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

std::optional<EmissionResult> emitMerchant(random::Rng &rng,
                                           const market::Market &market,
                                           const PaymentRoutingRules &policy,
                                           const ResolvedAccounts &resolved,
                                           const actors::Event &event) {
  const auto &commerce = market.commerce();
  const auto *catalog = commerce.catalog();
  if (catalog == nullptr) {
    return std::nullopt;
  }

  const std::uint32_t merchantIdx = pickMerchantIndex(
      rng, commerce, *event.spender, event.exploreP, policy.merchantRetryLimit);

  const auto &record = catalog->records[merchantIdx];
  const auto dst = record.counterpartyId;
  const auto category = record.category;

  const double rawAmount = math::amounts::merchantAmount(rng, category) *
                           event.spender->amountMultiplier;
  const double amount =
      primitives::utils::floorAndRound(rawAmount, /*floor=*/1.0);

  const auto route = selectPaymentRoute(rng, *event.spender);

  // Pre-resolved destination index for the merchant counterparty.
  const auto dstIdx = merchantIdx < resolved.merchantCounterpartyIdx.size()
                          ? resolved.merchantCounterpartyIdx[merchantIdx]
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

} // namespace PhantomLedger::spending::routing
