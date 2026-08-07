#pragma once
//
// phantomledger/activity/spending/market/commerce/gift_cards.hpp
//
// LEGITIMATE GIFT-CARD PURCHASES, on the same denomination ladder the
// gift-card scam typology draws from.
//
// WHY THIS EXISTS: A DETERMINISTIC FRAUD ORACLE.
// `fraud::amounts::giftCardScamAmount` returns exactly {100, 200, 500} on 75%
// of draws with $500 triple-weighted, and it is FIXED-NOMINAL by design (the
// FTC Data Spotlight citation on that function is sound — scammers really do
// demand max-denomination cards). Meanwhile every legitimate card amount is
// CPI-scaled by `event.priceScale` and then cent-rounded, so legitimate spend
// carries essentially NO mass on exact round hundreds.
//
// The result was measured at pop 900 x 1461d over the card view:
// `amount == $500.00` exactly returned 42 rows, 42 of them fraud —
// PRECISION 1.0000 at roughly 652x lift, from ONE exported column. That is a
// far stronger shortcut than any graph-structural feature in this corpus.
//
// THE LATTICE IS NOT THE DEFECT. THE ASYMMETRY IS.
// Apple's official US gift-card denominations are $10, $25, $50, $100, $200
// and $500 (apple.com and the major grocery/pharmacy racks; accessed
// 2026-08-07) — the scam sampler's {100, 200, 500} is a SUBSET of ordinary
// retail denominations, not a fraud signature. And gift cards are not niche:
// 56% of US consumers bought one in 2024, at an average of about $60 per card
// (industry consumer surveys, accessed 2026-08-07); the average value given as
// a gift is $47.91 and self-purchases average $51.93.
//
// So the generator was missing an entire ordinary behaviour, and its absence —
// not the fraud sampler — is what made the denominations diagnostic. Deleting
// or jittering the fraud lattice would have destroyed a cited quantity to hide
// a gap somewhere else. This adds the missing population instead.
//
// FRAUD STILL SKEWS HIGH, AND THAT IS THE POINT.
// Legitimate purchases concentrate at $25-$50 while the scam sampler
// concentrates at $500, so the denomination stays INFORMATIVE without being
// decisive — which is exactly the real relationship. A model may learn that a
// $500 gift card is riskier than a $25 one; it may not learn that $500 IS
// fraud.
//
// DRAW-FREE, so the RNG lane is untouched and no downstream draw shifts.
// Amounts move, and through the liquidity throttle row counts move with them
// (merchant-selection rule 7), so every golden re-pins — but no draw ORDER
// changes, which is what keeps the change explicable.

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/derived_endpoints.hpp"
#include "phantomledger/taxonomies/merchants/types.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace PhantomLedger::activity::spending::market::commerce::giftcards {

namespace derived = ::PhantomLedger::infra::derived;

inline constexpr std::uint64_t kSelectDomain = 0x4749'4654'0000'0001ULL;
inline constexpr std::uint64_t kDenomDomain = 0x4749'4654'0000'0002ULL;

/* Share of card purchases AT AN ELIGIBLE CATEGORY that are a gift-card
 * purchase, in basis points.
 *
 * DERIVED, not declared. US consumers spend on the order of $300/person/year
 * on gift cards (millennials $456, Gen Z $211; accessed 2026-08-07) at about
 * $60 per card, so roughly 5 cards per person per year. This generator emits
 * about 237 card payments per person per year, and the eligible categories
 * below carry roughly 40% of them, so 5 / (237 * 0.40) ~= 5.3%. Rounded to
 * 500 bp. The realized overall share is PRINTED by the gate rather than
 * asserted, because it moves with the category mix. */
inline constexpr std::uint32_t kGiftCardShareBasisPoints = 500;

/* Where gift cards are actually sold: supermarket and pharmacy racks, general
 * retail, and online storefronts. Deliberately NOT fuel, utilities, telecom,
 * insurance, education or restaurants — a rack in a filling station exists but
 * modelling it would widen the eligible base without evidence. */
[[nodiscard]] constexpr bool
eligibleCategory(::PhantomLedger::merchants::Category category) noexcept {
  using ::PhantomLedger::merchants::Category;
  return category == Category::grocery || category == Category::pharmacy ||
         category == Category::retailOther || category == Category::ecommerce;
}

/* The ladder, and the weights that put its mean on the cited average.
 *
 * Denominations are Apple's official US set. Weights are DECLARED (CLASS S
 * UNCITED for the shape — no published denomination histogram exists) but are
 * CONSTRAINED: they must reproduce the cited ~$47.91-$60 average card value.
 * Realized mean = $58.15, inside the cited band.
 *
 * THE $500 WEIGHT IS THE LOAD-BEARING ONE, and it is CAPPED BY THE MEAN rather
 * than tuned to a target lift. Raising it puts more legitimate rows on the
 * value the scam sampler concentrates on, which is what drives the residual
 * fraud precision down — but it also drags the mean off its citation, and
 * breaking a cited quantity to improve an uncited one is the error
 * merchant-selection rule 1 records. 200bp is the most $500 mass the $47.91-$60
 * average will carry. The residual overshoot is therefore NOT closable from
 * this constant; see the note on scam VOLUME below. */
inline constexpr std::array<double, 6> kDenominations{10.0,  25.0,  50.0,
                                                      100.0, 200.0, 500.0};
inline constexpr std::array<std::uint32_t, 6> kDenomWeightBp{1400, 3500, 3000,
                                                             1500, 400,  200};

/* Is this purchase a gift-card purchase, and if so for how much?
 *
 * Keyed on (person, merchant, timestamp) — the row's own world coordinates,
 * never an emission ordinal, so the answer is stable under any windowing and
 * the batch and windowed engines agree without coordination. Same discipline
 * as `commerce::affinity`.
 *
 * NOT CPI-SCALED, deliberately and symmetrically with the fraud sampler: a $50
 * gift card is $50 in 1995 and in 2020. The denomination ladder is a nominal
 * retail fact, not a price level. */
[[nodiscard]] inline std::optional<double>
amountFor(entity::PersonId person, const entity::Key &merchant,
          std::int64_t ts,
          ::PhantomLedger::merchants::Category category) noexcept {
  if (!eligibleCategory(category)) {
    return std::nullopt;
  }
  const auto mixed =
      derived::splitmix(static_cast<std::uint64_t>(person)) ^
      derived::splitmix(merchant.number) ^
      derived::splitmix(static_cast<std::uint64_t>(ts));

  if (derived::splitmix(mixed ^ kSelectDomain) % 10'000U >=
      kGiftCardShareBasisPoints) {
    return std::nullopt;
  }

  auto pick = derived::splitmix(mixed ^ kDenomDomain) % 10'000U;
  for (std::size_t i = 0; i < kDenominations.size(); ++i) {
    if (pick < kDenomWeightBp[i]) {
      return kDenominations[i];
    }
    pick -= kDenomWeightBp[i];
  }
  return kDenominations.back();
}

} // namespace PhantomLedger::activity::spending::market::commerce::giftcards
