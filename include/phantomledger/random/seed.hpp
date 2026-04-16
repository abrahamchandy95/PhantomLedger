#pragma once

#include "phantomledger/crypto/blake2b.hpp"

#include <array>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace PhantomLedger::random {

namespace detail {

template <typename Parts>
[[nodiscard]] inline std::string buildKey(std::uint64_t base,
                                          const Parts &parts) {
  std::string key = std::to_string(base);
  for (const auto &part : parts) {
    key.push_back('|');
    key.append(std::string_view{part});
  }
  return key;
}

// read 8 bytes in little-endian
[[nodiscard]] inline std::uint64_t
readU64Le(const std::array<std::uint8_t, 8> &bytes) noexcept {
  std::uint64_t value = 0;
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    value |= static_cast<std::uint64_t>(bytes[i]) << (i * 8U);
  }
  return value;
}

template <typename Parts>
[[nodiscard]] inline std::uint64_t derive(std::uint64_t base,
                                          const Parts &parts) {
  const std::string key = buildKey(base, parts);

  std::array<std::uint8_t, 8> bytes{};
  crypto::blake2b::digest(key.data(), key.size(), bytes.data(), bytes.size());

  return readU64Le(bytes);
}

} // namespace detail

[[nodiscard]] inline std::uint64_t
deriveSeed(std::uint64_t base, std::initializer_list<std::string_view> parts) {
  return detail::derive(base, parts);
}

[[nodiscard]] inline std::uint64_t
deriveSeed(std::uint64_t base, std::span<const std::string_view> parts) {
  return detail::derive(base, parts);
}

[[nodiscard]] inline std::uint64_t
deriveSeed(std::uint64_t base, const std::vector<std::string> &parts) {
  return detail::derive(base, parts);
}

} // namespace PhantomLedger::random
