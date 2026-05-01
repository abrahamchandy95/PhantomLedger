#pragma once

#include "phantomledger/entities/synth/pii/geonames.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace PhantomLedger::entities::synth::pii {

using namespace ::PhantomLedger::taxonomies::enums;

struct PoolSizes {
  std::size_t firstNames = 50'000;
  std::size_t middleNames = 50'000;
  std::size_t lastNames = 50'000;
  std::size_t streets = 50'000;
};

struct LocalePool {
  locale::Country country = locale::kDefaultCountry;

  std::vector<std::string> firstNames;
  std::vector<std::string> middleNames;
  std::vector<std::string> lastNames;

  std::vector<std::string> streets;

  std::vector<ZipEntry> zipTable;
};

/// Aggregate of all locale pools, indexed by `Country`.
struct PoolSet {
  std::array<LocalePool, locale::kCountryCount> byCountry{};

  [[nodiscard]] const LocalePool &forCountry(locale::Country c) const noexcept {
    return byCountry[toIndex(c)];
  }
};

[[nodiscard]] LocalePool buildLocalePool(locale::Country country,
                                         std::vector<ZipEntry> zips,
                                         PoolSizes sizes, std::uint32_t seed);

[[nodiscard]] LocalePool buildLocalePool(locale::Country country,
                                         PoolSizes sizes, std::uint32_t seed);

} // namespace PhantomLedger::entities::synth::pii
