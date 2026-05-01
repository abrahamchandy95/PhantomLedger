#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace PhantomLedger::locale {

enum class Country : std::uint8_t {
  us = 0, ///< United States
  gb,     ///< United Kingdom
  ca,     ///< Canada
  au,     ///< Australia
  de,     ///< Germany
  fr,     ///< France
  es,     ///< Spain
  it,     ///< Italy
  nl,     ///< Netherlands
  br,     ///< Brazil
  mx,     ///< Mexico
  in,     ///< India
  jp,     ///< Japan
  cn,     ///< China
  kr,     ///< South Korea
  ru,     ///< Russia
};

inline constexpr auto kCountries = std::to_array<Country>({
    Country::us,
    Country::gb,
    Country::ca,
    Country::au,
    Country::de,
    Country::fr,
    Country::es,
    Country::it,
    Country::nl,
    Country::br,
    Country::mx,
    Country::in,
    Country::jp,
    Country::cn,
    Country::kr,
    Country::ru,
});

inline constexpr std::size_t kCountryCount = kCountries.size();

inline constexpr Country kDefaultCountry = Country::us;

} // namespace PhantomLedger::locale
