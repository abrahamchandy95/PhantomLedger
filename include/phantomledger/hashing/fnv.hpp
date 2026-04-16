#pragma once

#include "phantomledger/hashing/constants.hpp"

#include <cstdint>
#include <string_view>

namespace PhantomLedger::hashing {

constexpr std::uint64_t fnv1a64(std::string_view value) noexcept {
  std::uint64_t hash = constants::fnv64_offset;
  for (unsigned char c : value) {
    hash ^= static_cast<std::uint64_t>(c);
    hash *= constants::fnv64_prime;
  }
  return hash;
}

} // namespace PhantomLedger::hashing
