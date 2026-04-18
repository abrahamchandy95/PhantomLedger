#pragma once

#include "phantomledger/crypto/blake2b.hpp"

#include <array>
#include <concepts>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>

namespace PhantomLedger::random {

namespace detail {

inline constexpr char seedSeparator = '|';
inline constexpr std::size_t seedBytes = 8;
inline constexpr unsigned bitsPerByte = 8U;

template <typename Parts>
[[nodiscard]] inline std::string buildKey(std::uint64_t base,
                                          const Parts &parts) {
  std::string key = std::to_string(base);
  for (const auto &part : parts) {
    key.push_back(seedSeparator);
    key.append(std::string_view{part});
  }
  return key;
}

[[nodiscard]] inline std::uint64_t
readU64Le(const std::array<std::uint8_t, seedBytes> &bytes) noexcept {
  std::uint64_t value = 0;
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    value |= static_cast<std::uint64_t>(bytes[i]) << (i * bitsPerByte);
  }
  return value;
}

template <typename Parts>
[[nodiscard]] inline std::optional<std::uint64_t> derive(std::uint64_t base,
                                                         const Parts &parts) {
  const std::string key = buildKey(base, parts);

  std::array<std::uint8_t, seedBytes> bytes{};
  const bool ok = crypto::blake2b::digest(key.data(), key.size(), bytes.data(),
                                          bytes.size());

  if (!ok) {
    return std::nullopt;
  }

  return readU64Le(bytes);
}

} // namespace detail

[[nodiscard]] inline std::optional<std::uint64_t>
deriveSeed(std::uint64_t base, std::initializer_list<std::string_view> parts) {
  return detail::derive(base, parts);
}

template <std::ranges::input_range Parts>
  requires std::convertible_to<std::ranges::range_reference_t<Parts>,
                               std::string_view>
[[nodiscard]] inline std::optional<std::uint64_t>
deriveSeed(std::uint64_t base, const Parts &parts) {
  return detail::derive(base, parts);
}

} // namespace PhantomLedger::random
