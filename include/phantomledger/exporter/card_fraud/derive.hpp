#pragma once
//
// phantomledger/exporter/card_fraud/derive.hpp
//
// Content-keyed, export-time derivations for the card-fraud use case —
// the attributes TF_GNN_v3 loads that the world model does not carry
// (use_chip, error, split flags, identifier strings, the category
// fallback for non-catalog destinations). Shared by the streaming leg
// and the vertex/edge finisher so both derive IDENTICAL values for the
// same entity or row.
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
// Every distribution below is a model value with a classed row in
// docs/fraud_model_audit.md (card-fraud-2026-07 block); change them
// only through a named model version.
//

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/hashing/constants.hpp"
#include "phantomledger/primitives/hashing/fnv.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/taxonomies/merchants/types.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cmath>
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

// The row's content key: timestamp, endpoints, amount in cents.
[[nodiscard]] inline std::uint64_t
rowHash(std::uint64_t lane, const transactions::Transaction &tx) noexcept {
  auto h = mixField(lane, static_cast<std::uint64_t>(tx.timestamp));
  h = mixKey(h, tx.source);
  h = mixKey(h, tx.target);
  return mixField(
      h, static_cast<std::uint64_t>(std::llround(tx.amount * 100.0)));
}

// --------------------------------------------------------- identifiers
//
// Identifier scheme (CHOICE): collision-free renderings of the full
// entity Key triple. C = credit card (the card Key), D = the account's
// derived debit card (the account Key), M = merchant/destination,
// P = party (person id), T = Payment_Transaction (corpus row_seq).

[[nodiscard]] inline std::string keyDigits(const entity::Key &k) {
  return std::to_string(static_cast<unsigned>(k.role)) + "." +
         std::to_string(static_cast<unsigned>(k.bank)) + "." +
         std::to_string(k.number);
}

[[nodiscard]] inline std::string cardId(const entity::Key &source,
                                        bool credit) {
  return (credit ? "C" : "D") + keyDigits(source);
}

[[nodiscard]] inline std::string merchantId(const entity::Key &destination) {
  return "M" + keyDigits(destination);
}

[[nodiscard]] inline std::string partyId(entity::PersonId person) {
  return "P" + std::to_string(person);
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

// ---------------------------------------------------------- splits
//
// Chronological train/val/test at .70/.15/.15 of the window's days
// (floored day boundaries; CHOICE). Settlement-tail rows past the
// window end land in test by construction.

struct SplitBounds {
  std::int64_t trainEndExcl = 0;
  std::int64_t valEndExcl = 0;
};

[[nodiscard]] inline SplitBounds splitBounds(time::Window window) noexcept {
  const auto days = static_cast<std::int64_t>(window.days);
  const auto trainDays = static_cast<int>((days * 70) / 100);
  const auto valDays = static_cast<int>((days * 85) / 100);
  return SplitBounds{
      .trainEndExcl =
          time::toEpochSeconds(time::addDays(window.start, trainDays)),
      .valEndExcl = time::toEpochSeconds(time::addDays(window.start, valDays)),
  };
}

struct SplitFlags {
  int train = 0;
  int val = 0;
  int test = 0;
};

[[nodiscard]] constexpr SplitFlags splitFor(std::int64_t ts,
                                            const SplitBounds &b) noexcept {
  if (ts < b.trainEndExcl) {
    return {.train = 1, .val = 0, .test = 0};
  }
  if (ts < b.valEndExcl) {
    return {.train = 0, .val = 1, .test = 0};
  }
  return {.train = 0, .val = 0, .test = 1};
}

} // namespace PhantomLedger::exporter::card_fraud::derive
