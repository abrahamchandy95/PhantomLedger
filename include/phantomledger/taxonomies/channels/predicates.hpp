#pragma once

#include "phantomledger/taxonomies/channels/names.hpp"
#include "phantomledger/taxonomies/enums.hpp"

#include <cstdint>

namespace PhantomLedger::channels {
namespace detail {

namespace enumTax = ::PhantomLedger::taxonomies::enums;

inline constexpr std::uint8_t kGroupMask = 0xF0;

[[nodiscard]] constexpr std::uint8_t groupByte(Tag value) noexcept {
  return static_cast<std::uint8_t>(value.value & kGroupMask);
}

template <ChannelEnum E>
[[nodiscard]] constexpr std::uint8_t groupByte(E value) noexcept {
  return static_cast<std::uint8_t>(enumTax::toByte(value) & kGroupMask);
}

template <ChannelEnum E>
[[nodiscard]] constexpr bool inGroup(Tag current, E representative) noexcept {
  return kKnown[current.value] &&
         groupByte(current) == groupByte(representative);
}

} // namespace detail

[[nodiscard]] constexpr bool isLegit(Tag value) noexcept {
  return detail::inGroup(value, Legit::salary);
}

[[nodiscard]] constexpr bool isRent(Tag value) noexcept {
  return detail::inGroup(value, Rent::generic);
}

[[nodiscard]] constexpr bool isFamily(Tag value) noexcept {
  return detail::inGroup(value, Family::allowance);
}

[[nodiscard]] constexpr bool isCredit(Tag value) noexcept {
  return detail::inGroup(value, Credit::payment);
}

[[nodiscard]] constexpr bool isProduct(Tag value) noexcept {
  return detail::inGroup(value, Product::mortgage);
}

[[nodiscard]] constexpr bool isGovernment(Tag value) noexcept {
  return detail::inGroup(value, Government::socialSecurity);
}

[[nodiscard]] constexpr bool isInsurance(Tag value) noexcept {
  return detail::inGroup(value, Insurance::premium);
}

[[nodiscard]] constexpr bool isFraud(Tag value) noexcept {
  return detail::inGroup(value, Fraud::classic);
}

[[nodiscard]] constexpr bool isCamouflage(Tag value) noexcept {
  return detail::inGroup(value, Camouflage::bill);
}

[[nodiscard]] constexpr bool isLiquidity(Tag value) noexcept {
  return detail::inGroup(value, Liquidity::overdraftFee);
}

[[nodiscard]] constexpr bool isKnown(Tag value) noexcept {
  return detail::kKnown[value.value];
}

[[nodiscard]] constexpr bool isPaydayInbound(Tag value) noexcept {
  return detail::kPaydayInbound[value.value];
}

// Card-like channels — channels whose settlement looks like a card
// transaction (immediate or near-immediate posting via the card rails).
// Cross-group predicate: not a single channel-group membership test.
[[nodiscard]] constexpr bool isCardLike(Tag value) noexcept {
  return is(value, Legit::atm) || is(value, Legit::merchant) ||
         is(value, Legit::cardPurchase) || is(value, Legit::p2p);
}

// Channels whose initial posting attempt may need to be retried (e.g.
// scheduled debits that can fail on insufficient funds and be reissued).
// Cross-group predicate spanning Legit, Rent, Insurance, and Product.
[[nodiscard]] constexpr bool isRetryable(Tag value) noexcept {
  return is(value, Legit::bill) || is(value, Legit::subscription) ||
         is(value, Legit::externalUnknown) || isRent(value) ||
         is(value, Insurance::premium) || is(value, Product::mortgage) ||
         is(value, Product::autoLoan) || is(value, Product::studentLoan) ||
         is(value, Product::taxEstimated);
}

} // namespace PhantomLedger::channels
