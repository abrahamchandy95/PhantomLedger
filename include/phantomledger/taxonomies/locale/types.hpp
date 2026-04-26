#pragma once

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
  in_,    ///< India (`in` is a C++ alternative-token keyword)
  jp,     ///< Japan
  cn,     ///< China
  kr,     ///< South Korea
  ru,     ///< Russia
};

inline constexpr std::size_t kCountryCount = 16;
inline constexpr Country kDefaultCountry = Country::us;

[[nodiscard]] constexpr std::size_t slot(Country c) noexcept {
  return static_cast<std::size_t>(c);
}

} // namespace PhantomLedger::locale
