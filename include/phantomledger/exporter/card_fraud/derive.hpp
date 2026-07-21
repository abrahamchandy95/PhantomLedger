#pragma once
//
// phantomledger/exporter/card_fraud/derive.hpp
//
// Content-keyed, export-time derivations for the card-fraud use case —
// the attributes TF_GNN_v3 loads that the world model does not carry
// (use_chip, error, identifier strings, gender, and the category
// fallback for non-catalog destinations). Merchant outlet geography and
// City population are NO LONGER derived here (geo-causal-v1): they are
// world-modeled (`entity::merchant::Record.location`, assigned in G1c)
// and resolved through `synth::geo::geography()` at export.
//
// Determinism rules:
//  * Every derivation hashes explicit fixed-width field bytes through
//    FNV-1a in little-endian order (mixField) — never std::hash, whose
//    layout is implementation-defined. Same corpus => same bytes on
//    every toolchain.
//  * Lanes are salted by name (kUseChipLane, kErrorLane, ...) so the
//    per-row draws are decorrelated.
//  * Byte-identical rows derive byte-identical attributes, which is
//    consistent: such rows are interchangeable by the ordering re-pin.
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
// docs/fraud_model_audit.md (card-fraud-2026-07 block); change them
// only through a named model version.
//

#include "phantomledger/encoding/render.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/hashing/constants.hpp"
#include "phantomledger/primitives/hashing/fnv.hpp"
#include "phantomledger/taxonomies/merchants/types.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
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

[[nodiscard]] inline std::string cardId(const entity::Key &source,
                                        bool credit) {
  const auto rendered = ::PhantomLedger::encoding::format(source);
  std::string out;
  out.reserve(1 + rendered.view().size());
  out.push_back(credit ? 'C' : 'D');
  out.append(rendered.view());
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
// TabFormer "Use Chip" value set; mix Swipe .63 / Chip .26 / Online .11
// (CHOICE, [Likely] vs the TabFormer empirical mix — owner verifies).

[[nodiscard]] inline std::string_view
useChipFor(const transactions::Transaction &tx) noexcept {
  const auto draw = rowHash(kUseChipLane, tx) % 10'000U;
  if (draw < 6'300U) {
    return "Swipe Transaction";
  }
  if (draw < 8'900U) {
    return "Chip Transaction";
  }
  return "Online Transaction";
}

// --------------------------------------------------------------- error
//
// TabFormer "Errors?" value set; incidence 2.0% of view rows, mix
// Insufficient Balance .40 / Bad PIN .20 / Technical Glitch .20 /
// Bad Card Number .08 / Bad Expiration .05 / Bad CVV .05 /
// Bad Zipcode .02 (CHOICE, [Likely] — owner verifies). Rows without an
// error carry the empty string, exactly like the source dataset.

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

} // namespace PhantomLedger::exporter::card_fraud::derive
