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
  entity::Key source{};
  channels::Tag channel{};
};

[[nodiscard]] PaymentRoute selectPaymentRoute(random::Rng &rng,
                                              const actors::Spender &spender) {
  if (!spender.hasCard) {
    return {spender.depositAccount, kMerchantChannel};
  }
  if (rng.coin(spender.persona->card.share)) {
    return {spender.card, kCardChannel};
  }
  return {spender.depositAccount, kMerchantChannel};
}

} // namespace

// ----------------------------- Bill --------------------------------

transactions::Transaction emitBill(random::Rng &rng,
                                   const market::Market &market,
                                   const Policy &policy,
                                   const actors::Event &event) {
  const auto &commerce = market.commerce();
  const auto billerRow = commerce.billers().rowOf(event.spender->personIndex);

  std::uint32_t billerIdx = 0;
  if (!billerRow.empty() && rng.coin(policy.preferBillersP)) {
    const auto slot = rng.choiceIndex(billerRow.size());
    billerIdx = billerRow[slot];
  } else {
    const auto &cdf = commerce.billerCdf();
    billerIdx = static_cast<std::uint32_t>(
        distributions::sampleIndex(cdf, rng.nextDouble()));
  }

  const auto *catalog = commerce.catalog();
  const entity::Key dst = catalog->records[billerIdx].counterpartyId;

  const double amount = math::amounts::kBill.sample(rng);

  return event.factory->make(transactions::Draft{
      .source = event.spender->depositAccount,
      .destination = dst,
      .amount = amount,
      .timestamp = time::toEpochSeconds(event.ts),
      .isFraud = 0,
      .ringId = -1,
      .channel = kBillChannel,
  });
}

// --------------------------- External ------------------------------

transactions::Transaction emitExternal(random::Rng &rng,
                                       const market::Market &market,
                                       const actors::Event &event) {
  // The merchant catalog does not currently expose an externals
  // roster (the Python `Merchants.externals` field has no analogue
  // on `entity::merchant::Catalog`). Route everything to the
  // documented sentinel destination until the entity layer adds it
  // back, optionally surfaced through `commerce::View`.
  (void)market; // reserved for the future View-routed path
  const entity::Key dst =
      entity::makeKey(entity::Role::merchant, entity::Bank::external, 1u);

  const double amount = primitives::utils::floorAndRound(
      math::amounts::kExternalUnknown.sample(rng),
      /*floor=*/1.0);

  return event.factory->make(transactions::Draft{
      .source = event.spender->depositAccount,
      .destination = dst,
      .amount = amount,
      .timestamp = time::toEpochSeconds(event.ts),
      .isFraud = 0,
      .ringId = -1,
      .channel = kExternalChannel,
  });
}

// ----------------------------- P2P ---------------------------------

std::optional<transactions::Transaction> emitP2p(random::Rng &rng,
                                                 const market::Market &market,
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

  const double raw = math::amounts::kP2P.sample(rng) *
                     event.spender->persona->cash.amountMultiplier;
  const double amount = primitives::utils::floorAndRound(raw, /*floor=*/1.0);

  return event.factory->make(transactions::Draft{
      .source = event.spender->depositAccount,
      .destination = dst,
      .amount = amount,
      .timestamp = time::toEpochSeconds(event.ts),
      .isFraud = 0,
      .ringId = -1,
      .channel = kP2pChannel,
  });
}

// --------------------------- Merchant ------------------------------

std::optional<transactions::Transaction>
emitMerchant(random::Rng &rng, const market::Market &market,
             const Policy &policy, const actors::Event &event) {
  const auto &commerce = market.commerce();
  const auto *catalog = commerce.catalog();
  if (catalog == nullptr) {
    return std::nullopt;
  }

  const std::uint32_t merchantIdx = pickMerchantIndex(
      rng, commerce, *event.spender, event.exploreP, policy.maxRetries);

  const auto &record = catalog->records[merchantIdx];
  const auto dst = record.counterpartyId;
  const auto category = record.category;

  const double rawAmount = math::amounts::merchantAmount(rng, category) *
                           event.spender->persona->cash.amountMultiplier;
  const double amount =
      primitives::utils::floorAndRound(rawAmount, /*floor=*/1.0);

  const auto route = selectPaymentRoute(rng, *event.spender);

  return event.factory->make(transactions::Draft{
      .source = route.source,
      .destination = dst,
      .amount = amount,
      .timestamp = time::toEpochSeconds(event.ts),
      .isFraud = 0,
      .ringId = -1,
      .channel = route.channel,
  });
}

} // namespace PhantomLedger::spending::routing
