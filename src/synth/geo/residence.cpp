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
    pool.weights = w;
    byCountry_.emplace(key, std::move(pool));
  }

  // relocation-2026-07: the same rows partitioned by state, for the in-state
  // branch of a move. `std::map` keeps iteration ordered so construction is
  // deterministic; the pools share the catalogue's row order for the same
  // reason the country pools do.
  std::map<std::pair<std::uint8_t, std::string>, std::vector<double>>
      stateWeights;
  std::map<std::pair<std::uint8_t, std::string>,
           std::vector<geoent::GeoAreaId>>
      stateIds;
  for (const auto &area : catalog.areas()) {
    const double w = residentialWeight(area);
    if (w <= 0.0 || area.stateCode.empty()) {
      continue;
    }
    const auto key = std::pair{countryKey(area.country), area.stateCode};
    stateWeights[key].push_back(w);
    stateIds[key].push_back(area.id);
  }
  for (auto &[key, w] : stateWeights) {
    CountryPool pool;
    pool.ids = std::move(stateIds[key]);
    pool.cdf = dist::buildCdf(w);
    pool.weights = std::move(w);
    byState_.emplace(key, std::move(pool));
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

geoent::GeoAreaId ResidenceSampler::sampleWith(double u,
                                               locale::Country country) const {
  const auto it = byCountry_.find(countryKey(country));
  if (it == byCountry_.end()) {
    return geoent::invalidGeoArea;
  }
  const auto &pool = it->second;
  return pool.ids[dist::sampleIndex(pool.cdf, u)];
}

geoent::GeoAreaId
ResidenceSampler::sampleInState(double u, locale::Country country,
                                std::string_view stateCode) const {
  const auto it =
      byState_.find(std::pair{countryKey(country), std::string{stateCode}});
  if (it == byState_.end()) {
    // The state has no residential area in the pinned catalogue. Reuse `u`
    // rather than drawing again — see the header note on fixed draw counts.
    return sampleWith(u, country);
  }
  const auto &pool = it->second;
  return pool.ids[dist::sampleIndex(pool.cdf, u)];
}

geoent::GeoAreaId
ResidenceSampler::pickExcluding(const CountryPool &pool, double u,
                               geoent::GeoAreaId exclude) noexcept {
  // Renormalise over the complement of `exclude` and walk. A linear walk is
  // correct and cheap here: the pools are per-state (a handful of rows) or the
  // national pool (a few hundred), and this runs once per MOVE, not per
  // transaction.
  double total = 0.0;
  for (std::size_t i = 0; i < pool.ids.size(); ++i) {
    if (pool.ids[i] != exclude) {
      total += pool.weights[i];
    }
  }
  if (total <= 0.0) {
    return geoent::invalidGeoArea; // the pool is the excluded area alone
  }

  double target = u * total;
  for (std::size_t i = 0; i < pool.ids.size(); ++i) {
    if (pool.ids[i] == exclude) {
      continue;
    }
    target -= pool.weights[i];
    if (target <= 0.0) {
      return pool.ids[i];
    }
  }
  // u == 1.0 (or float drift): fall to the last eligible row rather than
  // reporting "no area", which a caller would read as "the move failed".
  for (std::size_t i = pool.ids.size(); i-- > 0;) {
    if (pool.ids[i] != exclude) {
      return pool.ids[i];
    }
  }
  return geoent::invalidGeoArea;
}

geoent::GeoAreaId
ResidenceSampler::sampleExcluding(double u, locale::Country country,
                                  std::string_view stateCode,
                                  geoent::GeoAreaId exclude) const {
  if (!stateCode.empty()) {
    const auto it =
        byState_.find(std::pair{countryKey(country), std::string{stateCode}});
    // Two or more rows, or the exclusion leaves nothing to pick.
    if (it != byState_.end() && it->second.ids.size() > 1) {
      const auto picked = pickExcluding(it->second, u, exclude);
      if (geoent::validArea(picked)) {
        return picked;
      }
    }
  }
  const auto it = byCountry_.find(countryKey(country));
  if (it == byCountry_.end()) {
    return geoent::invalidGeoArea;
  }
  return pickExcluding(it->second, u, exclude);
}

} // namespace PhantomLedger::synth::geo
