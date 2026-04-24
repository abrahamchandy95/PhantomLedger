#pragma once
/*
 * Channel families are classified by the high nibble of the tag byte.
 *
 *   0x00 -> Legit
 *   0x10 -> Rent
 *   0x20 -> Family
 *   0x30 -> Credit
 *   0x40 -> Product
 *   0x50 -> Government
 *   0x60 -> Insurance
 *   0x70 -> Fraud
 *   0x80 -> Camouflage
 *   0x90 -> Liquidity (ledger-emitted: overdraft fees, LOC interest)
 *
 * Each group is limited to a single 16-value block.
 * If a family ever needs to grow beyond one block, this file will need to be
 * redesigned to use an explicit byte-to-family lookup table instead.
 *
 * We still consult kKnownTags so that unused values inside a block do not get
 * classified as valid channels. For example, 0x15 shares the Rent block, but
 * it is not considered Rent unless it is explicitly registered.
 */

#include "phantomledger/taxonomies/channels/names.hpp"

namespace PhantomLedger::channels {
namespace detail {

[[nodiscard]] constexpr std::uint8_t tagGroupByte(Tag t) noexcept {
  return static_cast<std::uint8_t>(t.value & 0xF0);
}

[[nodiscard]] constexpr bool isKnownGroup(Tag t,
                                          std::uint8_t groupByte) noexcept {
  return kKnownTags[t.value] && tagGroupByte(t) == groupByte;
}

} // namespace detail

[[nodiscard]] constexpr bool isLegit(Tag t) noexcept {
  return detail::isKnownGroup(t, 0x00);
}

[[nodiscard]] constexpr bool isRent(Tag t) noexcept {
  return detail::isKnownGroup(t, 0x10);
}

[[nodiscard]] constexpr bool isFamily(Tag t) noexcept {
  return detail::isKnownGroup(t, 0x20);
}

[[nodiscard]] constexpr bool isCredit(Tag t) noexcept {
  return detail::isKnownGroup(t, 0x30);
}

[[nodiscard]] constexpr bool isProduct(Tag t) noexcept {
  return detail::isKnownGroup(t, 0x40);
}

[[nodiscard]] constexpr bool isGovernment(Tag t) noexcept {
  return detail::isKnownGroup(t, 0x50);
}

[[nodiscard]] constexpr bool isInsurance(Tag t) noexcept {
  return detail::isKnownGroup(t, 0x60);
}

[[nodiscard]] constexpr bool isFraud(Tag t) noexcept {
  return detail::isKnownGroup(t, 0x70);
}

[[nodiscard]] constexpr bool isCamouflage(Tag t) noexcept {
  return detail::isKnownGroup(t, 0x80);
}

/// Liquidity events are ledger-emitted fee/interest transfers. The
/// ledger bypasses its insufficient-funds check for any channel in this
/// family so that fee collection and LOC interest can run against
/// accounts that are already overdrawn.
[[nodiscard]] constexpr bool isLiquidity(Tag t) noexcept {
  return detail::isKnownGroup(t, 0x90);
}

[[nodiscard]] constexpr bool isKnown(Tag t) noexcept {
  return detail::kKnownTags[t.value];
}

[[nodiscard]] constexpr bool isPaydayInbound(Tag t) noexcept {
  return detail::kPaydayInboundTags[t.value];
}

} // namespace PhantomLedger::channels
