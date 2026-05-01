#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace PhantomLedger::taxonomies::enums {

template <class T, class... Ts>
concept OneOf = (std::same_as<T, Ts> || ...);

template <class E>
concept ByteEnum = std::is_scoped_enum_v<E> &&
                   std::same_as<std::underlying_type_t<E>, std::uint8_t>;

template <ByteEnum E>
[[nodiscard]] constexpr std::uint8_t toByte(E value) noexcept {
  return std::to_underlying(value);
}

template <ByteEnum E>
[[nodiscard]] constexpr std::size_t toIndex(E value) noexcept {
  return static_cast<std::size_t>(std::to_underlying(value));
}

} // namespace PhantomLedger::taxonomies::enums
