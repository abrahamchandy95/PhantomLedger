#pragma once

#include "phantomledger/taxonomies/channels/names.hpp"

#include <cstdint>

namespace PhantomLedger::channels {
namespace detail {

[[nodiscard]] constexpr std::uint8_t groupByte(Tag t) noexcept {
  return static_cast<std::uint8_t>(t.value & 0xF0);
}

[[nodiscard]] constexpr bool inGroup(Tag t, std::uint8_t group) noexcept {
  return kKnown[t.value] && groupByte(t) == group;
}

} // namespace detail

[[nodiscard]] constexpr bool isLegit(Tag t) noexcept {
  return detail::inGroup(t, 0x00);
}

[[nodiscard]] constexpr bool isRent(Tag t) noexcept {
  return detail::inGroup(t, 0x10);
}

[[nodiscard]] constexpr bool isFamily(Tag t) noexcept {
  return detail::inGroup(t, 0x20);
}

[[nodiscard]] constexpr bool isCredit(Tag t) noexcept {
  return detail::inGroup(t, 0x30);
}

[[nodiscard]] constexpr bool isProduct(Tag t) noexcept {
  return detail::inGroup(t, 0x40);
}

[[nodiscard]] constexpr bool isGovernment(Tag t) noexcept {
  return detail::inGroup(t, 0x50);
}

[[nodiscard]] constexpr bool isInsurance(Tag t) noexcept {
  return detail::inGroup(t, 0x60);
}

[[nodiscard]] constexpr bool isFraud(Tag t) noexcept {
  return detail::inGroup(t, 0x70);
}

[[nodiscard]] constexpr bool isCamouflage(Tag t) noexcept {
  return detail::inGroup(t, 0x80);
}

[[nodiscard]] constexpr bool isLiquidity(Tag t) noexcept {
  return detail::inGroup(t, 0x90);
}

[[nodiscard]] constexpr bool isKnown(Tag t) noexcept {
  return detail::kKnown[t.value];
}

[[nodiscard]] constexpr bool isPaydayInbound(Tag t) noexcept {
  return detail::kPaydayInbound[t.value];
}
} // namespace PhantomLedger::channels
