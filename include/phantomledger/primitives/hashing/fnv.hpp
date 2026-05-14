#pragma once

#include "constants.hpp"

#include <cstdint>
#include <initializer_list>
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

constexpr std::uint64_t
fnv1a64Join(std::initializer_list<std::string_view> parts,
            char sep = '|') noexcept {
  std::uint64_t hash = constants::fnv64_offset;
  for (const auto part : parts) {
    for (unsigned char c : part) {
      hash ^= static_cast<std::uint64_t>(c);
      hash *= constants::fnv64_prime;
    }
    hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(sep));
    hash *= constants::fnv64_prime;
  }
  return hash;
}

} // namespace PhantomLedger::hashing
