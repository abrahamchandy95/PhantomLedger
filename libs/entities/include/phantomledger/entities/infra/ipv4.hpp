#pragma once
#include <cstdint>
#include <functional>

namespace PhantomLedger::network {

struct Ipv4 {
  std::uint32_t value = 0;

  auto operator<=>(const Ipv4 &) const = default;

  [[nodiscard]] static constexpr Ipv4 pack(std::uint8_t a, std::uint8_t b,
                                           std::uint8_t c,
                                           std::uint8_t d) noexcept {
    return Ipv4{(static_cast<std::uint32_t>(a) << 24U) |
                (static_cast<std::uint32_t>(b) << 16U) |
                (static_cast<std::uint32_t>(c) << 8U) |
                static_cast<std::uint32_t>(d)};
  }

  [[nodiscard]] constexpr std::uint8_t octet1() const noexcept {
    return static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
  }

  [[nodiscard]] constexpr std::uint8_t octet2() const noexcept {
    return static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
  }

  [[nodiscard]] constexpr std::uint8_t octet3() const noexcept {
    return static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
  }

  [[nodiscard]] constexpr std::uint8_t octet4() const noexcept {
    return static_cast<std::uint8_t>(value & 0xFFU);
  }
};

[[nodiscard]] inline std::size_t hashValue(const Ipv4 &value) noexcept {
  return std::hash<std::uint32_t>{}(value.value);
}

} // namespace PhantomLedger::network

namespace std {

template <> struct hash<PhantomLedger::network::Ipv4> {
  std::size_t
  operator()(const PhantomLedger::network::Ipv4 &value) const noexcept {
    return PhantomLedger::network::hashValue(value);
  }
};

} // namespace std
