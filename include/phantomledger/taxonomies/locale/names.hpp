#pragma once

#include "phantomledger/taxonomies/locale/types.hpp"

#include <array>
#include <string_view>

namespace PhantomLedger::locale {
namespace detail {

inline constexpr std::array<std::string_view, kCountryCount> kCountryCode{
    "US", "GB", "CA", "AU", "DE", "FR", "ES", "IT",
    "NL", "BR", "MX", "IN", "JP", "CN", "KR", "RU",
};

} // namespace detail

[[nodiscard]] constexpr std::string_view code(Country c) noexcept {
  return detail::kCountryCode[slot(c)];
}

[[nodiscard]] constexpr std::string_view geonamesFileStem(Country c) noexcept {
  return code(c);
}

} // namespace PhantomLedger::locale
