#include "phantomledger/synth/geo/residence.hpp"

#include "phantomledger/primitives/random/distributions/cdf.hpp"
#include "phantomledger/taxonomies/enums.hpp"

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace PhantomLedger::synth::geo {

namespace {

namespace geoent = ::PhantomLedger::entity::geography;
namespace dist = ::PhantomLedger::probability::distributions;
namespace enumTax = ::PhantomLedger::taxonomies::enums;

[[nodiscard]] std::uint8_t countryKey(locale::Country c) noexcept {
  return static_cast<std::uint8_t>(enumTax::toIndex(c));
}

} // namespace

ResidenceSampler::ResidenceSampler(const geoent::GeoCatalog &catalog) {
  // Group residential areas (weight > 0) by country, preserving catalogue
  // row order so the per-country CDF is deterministic.
  std::unordered_map<std::uint8_t, std::vector<double>> weights;
  std::unordered_map<std::uint8_t, std::vector<geoent::GeoAreaId>> ids;

  for (const auto &area : catalog.areas()) {
    const double w = residentialWeight(area);
    if (w <= 0.0) {
      continue; // PO-box / group-quarters / zero-residential: never a home
    }
    const auto key = countryKey(area.country);
    weights[key].push_back(w);
    ids[key].push_back(area.id);
  }

  for (auto &[key, w] : weights) {
    CountryPool pool;
    pool.ids = std::move(ids[key]);
    pool.cdf = dist::buildCdf(w); // w is non-empty and positive here
    byCountry_.emplace(key, std::move(pool));
  }
}

bool ResidenceSampler::has(locale::Country country) const noexcept {
  return byCountry_.find(countryKey(country)) != byCountry_.end();
}

geoent::GeoAreaId ResidenceSampler::sample(random::Rng &rng,
                                           locale::Country country) const {
  const auto it = byCountry_.find(countryKey(country));
  if (it == byCountry_.end()) {
    return geoent::invalidGeoArea;
  }
  const auto &pool = it->second;
  const auto idx = dist::sampleIndex(pool.cdf, rng.nextDouble());
  return pool.ids[idx];
}

} // namespace PhantomLedger::synth::geo
