#pragma once

#include "phantomledger/taxonomies/channels/names.hpp"
#include "phantomledger/taxonomies/enums.hpp"

#include <cstdint>

namespace PhantomLedger::channels {
namespace detail {

using namespace ::PhantomLedger::taxonomies::enums;

inline constexpr std::uint8_t kGroupMask = 0xF0;

[[nodiscard]] constexpr std::uint8_t groupByte(Tag value) noexcept {
  return static_cast<std::uint8_t>(value.value & kGroupMask);
}

template <ChannelEnum E>
[[nodiscard]] constexpr std::uint8_t groupByte(E value) noexcept {
  return static_cast<std::uint8_t>(toByte(value) & kGroupMask);
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

} // namespace PhantomLedger::channels
