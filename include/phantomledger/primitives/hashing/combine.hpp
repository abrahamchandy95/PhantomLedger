#pragma once

#include "constants.hpp"

#include <cstddef>
#include <functional>
#include <type_traits>

namespace PhantomLedger::hashing {

inline void combine(std::size_t &seed, std::size_t value) noexcept {
  const std::size_t mixed = value + constants::golden64;
  seed ^= mixed + (seed << 6U) + (seed >> 2U);
}

template <class T>
inline void combine(std::size_t &seed, const T &value) noexcept {
  using U = std::decay_t<T>;
  combine(seed, std::hash<U>{}(value));
}

template <class... Ts> inline std::size_t make(const Ts &...values) noexcept {
  std::size_t seed = 0;
  (combine(seed, values), ...);
  return seed;
}

} // namespace PhantomLedger::hashing
