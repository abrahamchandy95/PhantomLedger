#pragma once
//
// phantomledger/exporter/card_fraud/derive.hpp
//
// Export-time derivations for the card-fraud use case — the attributes
// TF_GNN_v3 loads that the world model does not carry directly
// (use_chip, error, identifier strings, gender, and the category
// fallback for non-catalog destinations). Merchant outlet geography and
// City population are NO LONGER derived here (geo-causal-v1): they are
// world-modeled (`entity::merchant::Record.location`, assigned in G1c)
// and resolved through `synth::geo::geography()` at export.
//
// use_chip is CAUSAL since ROUND 8 (use-chip-causal-2026-07): entry
// mode reads the destination's acceptance environment — the SAME
// Footprint axis the generator's own destination selection partitions
// on — plus the dated US EMV terminal mix for the chip/swipe split.
// Only `error`, gender and the category fallback remain content-keyed
// hashes.
//
// Determinism rules:
//  * Every content-keyed derivation hashes explicit fixed-width field
//    bytes through FNV-1a in little-endian order (mixField) — never
//    std::hash, whose layout is implementation-defined. Same corpus =>
//    same bytes on every toolchain.
//  * Lanes are salted by name (kUseChipLane, kErrorLane, ...) so the
//    per-row draws are decorrelated.
//  * Byte-identical rows derive byte-identical attributes, which is
//    consistent: such rows are interchangeable by the ordering re-pin.
//  * use_chip additionally reads the destination's STATIC catalog
//    footprint and the row's own calendar year — both observable at the
//    row's timestamp, so the point-in-time STREAM PREFIX property is
//    unchanged.
//
// Identifier scheme: entity Keys render through the project's
// canonical encoding::format, so card-fraud ids JOIN against every
// other exporter's tables. Credit/debit cards carry a C/D prefix over
// the canonical rendering (cards and accounts are distinct id spaces);
// merchants are the canonical counterparty rendering unchanged;
// parties are the canonical customer id (common::renderCustomerId at
// the call sites); Payment_Transaction is T<row_seq>.
//
// Every distribution below is a model value with a classed row in
// docs/fraud_model_audit.md (card-fraud-2026-07 block and the
// use-chip-causal-2026-07 amendment); change them only through a named
// model version.
//

#include "phantomledger/encoding/render.hpp"
#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/hashing/constants.hpp"
#include "phantomledger/primitives/hashing/fnv.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/taxonomies/merchants/types.hpp"
#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace PhantomLedger::exporter::card_fraud::derive {

// ------------------------------------------------------------- hashing

// FNV-1a over the 8 bytes of v in little-endian order — platform-fixed,
// unlike hashing::make (which routes through std::hash).
[[nodiscard]] constexpr std::uint64_t mixField(std::uint64_t h,
                                               std::uint64_t v) noexcept {
  for (int i = 0; i < 8; ++i) {
    h ^= (v >> (8 * i)) & 0xFFU;
    h *= ::PhantomLedger::hashing::constants::fnv64_prime;
  }
  return h;
}

[[nodiscard]] constexpr std::uint64_t mixKey(std::uint64_t h,
                                             const entity::Key &k) noexcept {
  h = mixField(h, (static_cast<std::uint64_t>(k.role) << 8) |
                      static_cast<std::uint64_t>(k.bank));
  return mixField(h, k.number);
}

inline constexpr std::uint64_t kUseChipLane =
    ::PhantomLedger::hashing::fnv1a64("card_fraud/use_chip");
inline constexpr std::uint64_t kErrorLane =
    ::PhantomLedger::hashing::fnv1a64("card_fraud/error");
inline constexpr std::uint64_t kCategoryLane =
    ::PhantomLedger::hashing::fnv1a64("card_fraud/merchant_category");
inline constexpr std::uint64_t kGenderLane =
    ::PhantomLedger::hashing::fnv1a64("card_fraud/party_gender");

// The row's content key: timestamp, endpoints, amount in cents.
[[nodiscard]] inline std::uint64_t
rowHash(std::uint64_t lane, const transactions::Transaction &tx) noexcept {
  auto h = mixField(lane, static_cast<std::uint64_t>(tx.timestamp));
  h = mixKey(h, tx.source);
  h = mixKey(h, tx.target);
  return mixField(h,
                  static_cast<std::uint64_t>(std::llround(tx.amount * 100.0)));
}

// --------------------------------------------------------- identifiers

// card-churn-2026-07: the card number is now a function of the account AND
// the GENERATION live at the row's timestamp, because a cardholder receives
// replacement cards. `entity::card::reissue` owns the schedule.
//
// GENERATION 0 RENDERS EXACTLY AS BEFORE — no suffix — and that is a
// deliberate containment choice, not laziness. Generation 0 IS the
// originally issued card, so the un-suffixed form remains truthful; and
// because short windows churn almost nothing (measured 1.2% over 60 days),
// every existing configuration keeps ~99% of its card ids byte-identical.
// Suffixing generation 0 too would have rewritten every card id in every
// table for no modelling gain.
[[nodiscard]] inline std::string cardId(const entity::Key &source, bool credit,
                                        std::uint32_t generation = 0) {
  const auto rendered = ::PhantomLedger::encoding::format(source);
  std::string out;
  out.reserve(1 + rendered.view().size() + 4);
  out.push_back(credit ? 'C' : 'D');
  out.append(rendered.view());
  if (generation > 0) {
    out.push_back('-');
    out.push_back('G');
    out.append(std::to_string(generation));
  }
  return out;
}

[[nodiscard]] inline std::string merchantId(const entity::Key &destination) {
  return std::string{::PhantomLedger::encoding::format(destination).view()};
}

[[nodiscard]] inline std::string txnId(std::uint64_t rowSeq) {
  return "T" + std::to_string(rowSeq);
}

// ------------------------------------------------------------ use_chip
//
// TabFormer "Use Chip" value set, CAUSAL since ROUND 8
// (use-chip-causal-2026-07). Entry mode is a property of the ACCEPTANCE
// ENVIRONMENT, which is exactly the axis the generator already
// partitions on:
//
//   * Legitimate selection (payments.cpp pickMerchantIndex) and the
//     fraud rails (unauthorized.cpp pickMerchantDestination) both split
//     card-not-present selection over the `Footprint::online`
//     population and card-present selection over physically-located
//     outlets. The destination therefore CARRIES the modality that was
//     decided at generation; deriving from it exports that decision
//     instead of overwriting it with a hash.
//   * A physically-located outlet (localOutlet / regionalOutlet /
//     nationalService — every non-online footprint receives a real
//     GeoArea in placeGeography) is a terminal: Chip or Swipe.
//   * Non-catalog view destinations (the fraud rails' degraded biller
//     fallback — remote-billed hub accounts by construction) are remote
//     acceptance endpoints: Online (DECLARED CHOICE).
//
// The chip/swipe split on physical rows follows the dated US EMV
// migration: zero before 2012 (mag-stripe era), low single digits
// through the October 2015 network liability shift, majority by 2018,
// ~0.90 by 2024, frozen outside coverage exactly like the era scales.
// Values are a DECLARED CHOICE shaped by the EMVCo US chip-transaction
// share series and the networks' liability-shift milestones ([Likely]
// — owner verifies); the per-row draw stays content-keyed on
// kUseChipLane, so byte-identical rows still derive identically and no
// generation randomness is consumed.
//
// This is a presentation-layer terminal-technology mix, NOT a per-card
// or per-terminal adoption state; effective card/terminal lifecycles
// remain a registered benchmark gate.

inline constexpr std::string_view kUseChipOnline = "Online Transaction";
inline constexpr std::string_view kUseChipChip = "Chip Transaction";
inline constexpr std::string_view kUseChipSwipe = "Swipe Transaction";

// Chip share of card-present transactions, in basis points of 10,000,
// by calendar year. Clamped: 0 before kFirstChipYear, frozen at the
// last entry after coverage.
[[nodiscard]] constexpr std::uint32_t
chipShareBasisPoints(std::int32_t year) noexcept {
  constexpr std::int32_t kFirstChipYear = 2012;
  constexpr std::array<std::uint16_t, 13> kChipShareBp{
      100,   // 2012 early dual-interface issuance
      200,   // 2013
      300,   // 2014
      1'000, // 2015 October liability shift
      3'000, // 2016
      4'500, // 2017
      5'500, // 2018 chip-on-chip majority
      6'500, // 2019
      7'300, // 2020
      8'000, // 2021
      8'500, // 2022
      8'800, // 2023
      9'000, // 2024 — frozen outside coverage
  };
  if (year < kFirstChipYear) {
    return 0U;
  }
  const auto offset = std::min<std::int64_t>(
      year - kFirstChipYear, static_cast<std::int64_t>(kChipShareBp.size()) - 1);
  return kChipShareBp[static_cast<std::size_t>(offset)];
}

// `footprint` is the destination's catalog footprint, or nullopt for a
// non-catalog view destination. The caller (streaming.hpp) resolves it
// from the same catalog index that resolves mer_cat, so the two columns
// agree on what the destination IS by construction.
[[nodiscard]] inline std::string_view
useChipFor(const transactions::Transaction &tx,
           std::optional<entity::merchant::Footprint> footprint) noexcept {
  if (!footprint.has_value() ||
      *footprint == entity::merchant::Footprint::online) {
    return kUseChipOnline;
  }
  const auto year = ::PhantomLedger::time::toCalendarDate(
                        ::PhantomLedger::time::fromEpochSeconds(tx.timestamp))
                        .year;
  const auto draw = rowHash(kUseChipLane, tx) % 10'000U;
  return draw < chipShareBasisPoints(year) ? kUseChipChip : kUseChipSwipe;
}

// --------------------------------------------------------------- error
//
// TabFormer "Errors?" value set; incidence 2.0% of view rows, mix
// Insufficient Balance .40 / Bad PIN .20 / Technical Glitch .20 /
// Bad Card Number .08 / Bad Expiration .05 / Bad CVV .05 /
// Bad Zipcode .02 (CHOICE, [Likely] — owner verifies). Rows without an
// error carry the empty string, exactly like the source dataset.
// STILL A CONTENT HASH: authorization attempts are not modelled, so
// this column has no cause behind it — the remaining half of online-GNN
// gate 4 (docs/card_fraud_online_gnn.md), tracked realism debt.

[[nodiscard]] inline std::string_view
errorFor(const transactions::Transaction &tx) noexcept {
  const auto h = rowHash(kErrorLane, tx);
  if (h % 10'000U >= 200U) {
    return "";
  }
  const auto pick = (h / 10'000U) % 10'000U;
  if (pick < 4'000U) {
    return "Insufficient Balance";
  }
  if (pick < 6'000U) {
    return "Bad PIN";
  }
  if (pick < 8'000U) {
    return "Technical Glitch";
  }
  if (pick < 8'800U) {
    return "Bad Card Number";
  }
  if (pick < 9'300U) {
    return "Bad Expiration";
  }
  if (pick < 9'800U) {
    return "Bad CVV";
  }
  return "Bad Zipcode";
}

// ---------------------------------------------- category fallback
//
// Destinations outside the merchant catalog (the fraud rail draws from
// the blueprint's biller accounts) still become Merchant vertices;
// their category is a content-keyed uniform pick over the taxonomy
// (CHOICE). Keyed by the destination, so the streamed mer_cat and the
// finisher's Merchant_Assigned edge agree by construction.

[[nodiscard]] inline merchants::Category
fallbackCategory(const entity::Key &destination) noexcept {
  const auto h = mixKey(kCategoryLane, destination);
  return merchants::kCategories[h % merchants::kCategoryCount];
}

// -------------------------------------------------------------- gender
//
// Gender is not modeled anywhere in the world (names are pool indices
// with no gender attribute). Party.gender is a content-keyed even
// F/M split per person (CHOICE; owner may replace with a modeled
// attribute later).

[[nodiscard]] inline std::string_view genderFor(entity::PersonId person) {
  return (mixField(kGenderLane, person) % 2U) == 0U ? "F" : "M";
}

// --------------------------------------------------------- coordinates
//
// The world stores coordinates as INTEGER MICRODEGREES and forbids double
// coordinates inside the model (entities/geography/area.hpp). TF_GNN_v3
// declares lat/lon as DOUBLE, so the conversion happens HERE, at the
// boundary, and nowhere else.
//
// Not a content-keyed derivation and not a hash — a pure unit change on a
// value the world already holds, so it is exact for every catalogue row
// (a microdegree integer is far inside double's exact-integer range) and
// renders through csv::Writer's shortest-round-trip formatter.
[[nodiscard]] constexpr double degreesFromE6(std::int32_t microdegrees) noexcept {
  return static_cast<double>(microdegrees) / 1'000'000.0;
}

} // namespace PhantomLedger::exporter::card_fraud::derive
