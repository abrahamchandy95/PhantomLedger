#pragma once

#include "ipv4.hpp"

#include <array>
#include <charconv>
#include <cstdint>
#include <string_view>

namespace PhantomLedger::network {

struct Ipv4String {
  std::array<char, 16> bytes{};
  std::uint8_t length = 0;

  [[nodiscard]] constexpr std::string_view view() const noexcept {
    return std::string_view{bytes.data(), length};
  }

  [[nodiscard]] constexpr bool empty() const noexcept { return length == 0; }

  constexpr operator std::string_view() const noexcept { return view(); }
};

[[nodiscard]] inline Ipv4String format(Ipv4 ip) noexcept {
  Ipv4String out;
  char *cursor = out.bytes.data();
  char *const end = out.bytes.data() + out.bytes.size();

  const auto putOctet = [&](std::uint8_t v) noexcept {
    const auto r = std::to_chars(cursor, end, v);
    cursor = r.ptr;
  };

  putOctet(ip.octet1());
  *cursor++ = '.';
  putOctet(ip.octet2());
  *cursor++ = '.';
  putOctet(ip.octet3());
  *cursor++ = '.';
  putOctet(ip.octet4());

  out.length = static_cast<std::uint8_t>(cursor - out.bytes.data());
  return out;
}

} // namespace PhantomLedger::network
