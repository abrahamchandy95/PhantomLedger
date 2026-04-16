#pragma once

#include <cstddef>
#include <cstdint>

namespace PhantomLedger::hashing::constants {

inline constexpr std::uint64_t fnv64_offset = 14695981039346656037ULL;
inline constexpr std::uint64_t fnv64_prime = 1099511628211ULL;

inline constexpr std::size_t golden64 =
    static_cast<std::size_t>(0x9e3779b97f4a7c15ULL);

} // namespace PhantomLedger::hashing::constants
