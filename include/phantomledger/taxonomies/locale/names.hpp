#pragma once

#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"

#include <array>
#include <string_view>

namespace PhantomLedger::locale {

using namespace ::PhantomLedger::taxonomies::enums;

namespace detail {

static_assert(isIndexable(kCountries));

inline constexpr auto kCountryCodes = std::to_array<std::string_view>({
    "US",
    "GB",
    "CA",
    "AU",
    "DE",
    "FR",
    "ES",
    "IT",
    "NL",
    "BR",
    "MX",
    "IN",
    "JP",
    "CN",
    "KR",
    "RU",
});

static_assert(kCountryCodes.size() == kCountryCount);

} // namespace detail

[[nodiscard]] constexpr std::string_view code(Country country) noexcept {
  return detail::kCountryCodes[toIndex(country)];
}

[[nodiscard]] constexpr std::string_view
geonamesFileStem(Country country) noexcept {
  return code(country);
}

} // namespace PhantomLedger::locale
