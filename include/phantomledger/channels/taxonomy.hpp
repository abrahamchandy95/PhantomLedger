#pragma once

#include <compare>
#include <concepts>
#include <cstdint>

namespace PhantomLedger::channels {

struct Tag {
  std::uint8_t value = 0;
  constexpr std::strong_ordering operator<=>(const Tag &) const = default;
};

inline constexpr Tag none{0};

enum class Legit : std::uint8_t {
  salary = 0x01,
  merchant = 0x02,
  cardPurchase = 0x03,
  bill = 0x04,
  p2p = 0x05,
  externalUnknown = 0x06,
  atm = 0x07,
  selfTransfer = 0x08,
  subscription = 0x09,
  clientAchCredit = 0x0A,
  cardSettlement = 0x0B,
  platformPayout = 0x0C,
  ownerDraw = 0x0D,
  investmentInflow = 0x0E,
};

enum class Rent : std::uint8_t {
  generic = 0x10,
  ach = 0x11,
  portal = 0x12,
  p2p = 0x13,
  check = 0x14,
};

enum class Family : std::uint8_t {
  allowance = 0x20,
  tuition = 0x21,
  support = 0x22,
  spouseTransfer = 0x23,
  parentGift = 0x24,
  siblingTransfer = 0x25,
  grandparentGift = 0x26,
  inheritance = 0x27,
};

enum class Credit : std::uint8_t {
  payment = 0x30,
  interest = 0x31,
  lateFee = 0x32,
  refund = 0x33,
  chargeback = 0x34,
};

enum class Product : std::uint8_t {
  mortgage = 0x40,
  autoLoan = 0x41,
  studentLoan = 0x42,
  taxEstimated = 0x43,
  taxBalanceDue = 0x44,
  taxRefund = 0x45,
};

enum class Government : std::uint8_t {
  socialSecurity = 0x50,
  pension = 0x51,
  disability = 0x52,
};

enum class Insurance : std::uint8_t {
  premium = 0x60,
  claim = 0x61,
};

enum class Fraud : std::uint8_t {
  classic = 0x70,
  cycle = 0x71,
  layeringIn = 0x72,
  layeringHop = 0x73,
  layeringOut = 0x74,
  funnelIn = 0x75,
  funnelOut = 0x76,
  structuring = 0x77,
  invoice = 0x78,
  muleIn = 0x79,
  muleForward = 0x7A,
};

enum class Camouflage : std::uint8_t {
  bill = 0x80,
  p2p = 0x81,
  salary = 0x82,
};

template <class T, class... Ts>
concept ChannelType = (std::same_as<T, Ts> || ...);

template <class T>
concept ChannelEnum = ChannelType<T, Legit, Rent, Family, Credit, Product,
                                  Government, Insurance, Fraud, Camouflage>;

template <ChannelEnum Enum>
[[nodiscard]] constexpr std::uint8_t toByte(Enum v) noexcept {
  return static_cast<std::uint8_t>(v);
}

template <ChannelEnum Enum> [[nodiscard]] constexpr Tag tag(Enum v) noexcept {
  return Tag{toByte(v)};
}

template <ChannelEnum Enum>
[[nodiscard]] constexpr bool is(Tag c, Enum v) noexcept {
  return c == tag(v);
}
} // namespace PhantomLedger::channels
