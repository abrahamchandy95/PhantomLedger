#pragma once

#include "phantomledger/synth/pii/geonames.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace PhantomLedger::synth::pii {

namespace enumTax = ::PhantomLedger::taxonomies::enums;

struct PoolSizes {
  std::size_t firstNames = 50'000;
  std::size_t middleNames = 50'000;
  std::size_t lastNames = 50'000;
  std::size_t streets = 50'000;
  std::size_t businessNames = 10'000;
};

struct LocalePool {
  locale::Country country = locale::kDefaultCountry;

  std::vector<std::string> firstNames;
  std::vector<std::string> middleNames;
  std::vector<std::string> lastNames;

  std::vector<std::string> streets;

  std::vector<std::string> businessNames;

  std::vector<ZipEntry> zipTable;
};

struct PoolSet {
  std::array<LocalePool, locale::kCountryCount> byCountry{};

  [[nodiscard]] const LocalePool &forCountry(locale::Country c) const noexcept {
    const auto &pool = byCountry[enumTax::toIndex(c)];

    assert(pool.country == c &&
           "PoolSet::forCountry: locale pool slot is unfilled (country tag "
           "mismatch). buildDefaultPoolSet (or equivalent) did not populate "
           "this country, but the LocaleMix routes records here. Fill every "
           "country with positive weight in the mix.");

    assert(
        !pool.firstNames.empty() &&
        "PoolSet::forCountry: locale pool has no firstNames. Slot was "
        "tagged with the right country but never filled by buildLocalePool.");

    return pool;
  }
};

[[nodiscard]] LocalePool buildLocalePool(locale::Country country,
                                         std::vector<ZipEntry> zips,
                                         PoolSizes sizes, std::uint32_t seed);

[[nodiscard]] LocalePool buildLocalePool(locale::Country country,
                                         PoolSizes sizes, std::uint32_t seed);

} // namespace PhantomLedger::synth::pii
