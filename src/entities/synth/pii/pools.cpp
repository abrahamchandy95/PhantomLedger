#include "phantomledger/entities/synth/pii/pools.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"
#include "phantomledger/taxonomies/locale/us_cities.hpp"
#include "phantomledger/taxonomies/locale/us_state.hpp"

#include "faker-cxx/generator.h"
#include "faker-cxx/location.h"
#include "faker-cxx/person.h"

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <utility>

namespace PhantomLedger::entities::synth::pii {
namespace {

[[nodiscard]] faker::Locale fakerLocale(locale::Country c) noexcept {
  using faker::Locale;
  using locale::Country;
  switch (c) {
  case Country::us:
    return Locale::en_US;
  case Country::gb:
    return Locale::en_GB;
  case Country::ca:
    return Locale::en_CA;
  case Country::au:
    return Locale::en_AU;
  case Country::de:
    return Locale::de_DE;
  case Country::fr:
    return Locale::fr_FR;
  case Country::es:
    return Locale::es_ES;
  case Country::it:
    return Locale::it_IT;
  case Country::nl:
    return Locale::nl_NL;
  case Country::br:
    return Locale::pt_BR;
  case Country::mx:
    return Locale::es_MX;
  case Country::in:
    return Locale::en_IN;
  case Country::jp:
    return Locale::ja_JP;
  case Country::cn:
    return Locale::zh_CN;
  case Country::kr:
    return Locale::ko_KR;
  case Country::ru:
    return Locale::ru_RU;
  }
  return Locale::en_US;
}

[[nodiscard]] bool hasMiddleNames(locale::Country c) noexcept {
  using locale::Country;

  return c != Country::jp && c != Country::cn && c != Country::kr;
}

template <typename Gen>
void fillWithReplacement(std::vector<std::string> &out, std::size_t count,
                         Gen &&gen) {
  out.clear();
  out.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    out.emplace_back(gen());
  }
}

} // namespace

LocalePool buildLocalePool(locale::Country country, std::vector<ZipEntry> zips,
                           PoolSizes sizes, std::uint32_t seed) {
  faker::setSeed(seed);

  const auto loc = fakerLocale(country);

  LocalePool pool;
  pool.country = country;
  pool.zipTable = std::move(zips);

  fillWithReplacement(pool.firstNames, sizes.firstNames,
                      [loc] { return faker::person::firstName(loc); });

  if (hasMiddleNames(country)) {

    fillWithReplacement(pool.middleNames, sizes.middleNames,
                        [loc] { return faker::person::firstName(loc); });
  }

  fillWithReplacement(pool.lastNames, sizes.lastNames,
                      [loc] { return faker::person::lastName(loc); });

  fillWithReplacement(pool.streets, sizes.streets,
                      [loc] { return faker::location::streetAddress(loc); });

  return pool;
}

namespace {

[[nodiscard]] std::string formatZip5(std::uint32_t zip) {
  std::string out(5, '0');
  for (int i = 4; i >= 0 && zip > 0; --i) {
    out[static_cast<std::size_t>(i)] = static_cast<char>('0' + (zip % 10));
    zip /= 10;
  }
  return out;
}

[[nodiscard]] std::vector<ZipEntry> synthesizeUsZipTable(std::size_t entryCount,
                                                         std::uint32_t seed) {
  std::vector<ZipEntry> out;
  if (entryCount == 0)
    return out;
  out.reserve(entryCount);

  // Compute total population weight.
  std::uint64_t totalWeight = 0;
  for (std::size_t i = 0; i < locale::us::kStateCount; ++i) {
    totalWeight +=
        locale::us::populationBasisPoints(static_cast<locale::us::State>(i));
  }

  std::mt19937 rng(seed);

  for (std::size_t i = 0; i < entryCount; ++i) {
    // Pick a state weighted by population.
    std::uniform_int_distribution<std::uint64_t> popDist(0, totalWeight - 1);
    const auto draw = popDist(rng);
    std::uint64_t acc = 0;
    auto state = locale::us::State::ca; // sane default
    for (std::size_t s = 0; s < locale::us::kStateCount; ++s) {
      acc +=
          locale::us::populationBasisPoints(static_cast<locale::us::State>(s));
      if (draw < acc) {
        state = static_cast<locale::us::State>(s);
        break;
      }
    }

    // Pick a ZIP inside that state's USPS-allocated range.
    const auto range = locale::us::zipRangeFor(state);
    std::uniform_int_distribution<std::uint32_t> zipDist(range.low, range.high);
    const auto zip = zipDist(rng);

    const auto cities = locale::us::citiesFor(state);
    std::uniform_int_distribution<std::size_t> cityDist(0, cities.size() - 1);
    const auto city = cities[cityDist(rng)];

    ZipEntry entry;
    entry.postalCode = formatZip5(zip);
    entry.city = std::string(city);
    entry.adminName = std::string(locale::us::fullName(state));
    entry.adminCode = std::string(locale::us::abbrev(state));
    out.push_back(std::move(entry));
  }
  return out;
}

[[nodiscard]] std::vector<ZipEntry>
synthesizeNonUsZipTable(faker::Locale loc, std::size_t entryCount) {
  std::vector<ZipEntry> out;
  out.reserve(entryCount);
  for (std::size_t i = 0; i < entryCount; ++i) {
    ZipEntry entry;
    entry.postalCode = faker::location::zipCode(loc);
    entry.city = faker::location::city(loc);
    entry.adminName = faker::location::state(loc);

    out.push_back(std::move(entry));
  }
  return out;
}

} // namespace

LocalePool buildLocalePool(locale::Country country, PoolSizes sizes,
                           std::uint32_t seed) {
  faker::setSeed(seed);

  const auto loc = fakerLocale(country);

  LocalePool pool;
  pool.country = country;

  const std::size_t zipCount = sizes.streets;
  if (country == locale::Country::us) {
    pool.zipTable = synthesizeUsZipTable(zipCount, seed);
  } else {
    pool.zipTable = synthesizeNonUsZipTable(loc, zipCount);
  }

  fillWithReplacement(pool.firstNames, sizes.firstNames,
                      [loc] { return faker::person::firstName(loc); });

  if (hasMiddleNames(country)) {
    fillWithReplacement(pool.middleNames, sizes.middleNames,
                        [loc] { return faker::person::firstName(loc); });
  }

  fillWithReplacement(pool.lastNames, sizes.lastNames,
                      [loc] { return faker::person::lastName(loc); });

  fillWithReplacement(pool.streets, sizes.streets,
                      [loc] { return faker::location::streetAddress(loc); });

  return pool;
}

} // namespace PhantomLedger::entities::synth::pii
