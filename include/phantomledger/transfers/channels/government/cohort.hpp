#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entropy/crypto/blake2b.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

namespace PhantomLedger::transfers::government::cohort {

namespace detail {

inline constexpr std::size_t kDigestBytes = 8;

[[nodiscard]] inline std::uint64_t stableU64(std::string_view a,
                                             std::string_view b) noexcept {
  std::array<std::uint8_t, 256> buf{};
  std::size_t n = 0;

  const auto push = [&](std::string_view s) {
    const auto take = std::min(s.size(), buf.size() - n);
    std::memcpy(buf.data() + n, s.data(), take);
    n += take;
  };

  push(a);
  if (n < buf.size()) {
    buf[n++] = '|';
  }
  push(b);
  if (n < buf.size()) {
    buf[n++] = '|';
  }

  std::uint8_t digest[kDigestBytes]{};
  const auto ok = crypto::blake2b::digest(buf.data(), n, digest, kDigestBytes);
  if (!ok) {
    return 0;
  }

  std::uint64_t v = 0;
  std::memcpy(&v, digest, sizeof(v));
  return v;
}

} // namespace detail

[[nodiscard]] inline int syntheticBirthDay(entity::PersonId person) noexcept {
  std::array<char, 24> buf{};
  const int n = std::snprintf(buf.data(), buf.size(), "%u",
                              static_cast<std::uint32_t>(person));
  const std::string_view pid{buf.data(),
                             static_cast<std::size_t>(std::max(n, 0))};
  return 1 +
         static_cast<int>((detail::stableU64(pid, "identity") >> 16U) % 28ULL);
}

} // namespace PhantomLedger::transfers::government::cohort
