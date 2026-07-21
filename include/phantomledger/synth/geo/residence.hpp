#pragma once
//
// phantomledger/synth/geo/residence.hpp
//
// Population-weighted home placement over the pinned geographic
// catalogue (geo-causal-v1, G1). A person lives where people actually
// live: the residential weight of an area is its ACS population, so a
// dense metro ZCTA is chosen far more often than a tiny rural one —
// replacing the old uniform `zipTableIdx` draw (a rural area had the
// same odds as Manhattan).
//
// Zero-residential areas (PO-box / corporate-only / group-quarters)
// carry weight 0 and are never selected as a home. Sampling is per
// COUNTRY: a person only receives a home in an area of their own
// country; if the catalogue has no residential area for that country,
// the caller gets `invalidGeoArea`.
//
// Built ONCE per run (per-country CDFs cached) and sampled on an
// isolated RNG lane so home placement never perturbs the shared entity
// stream (STANDING LAW: isolate new randomness on named lanes).
//

#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace PhantomLedger::synth::geo {

// Residential weight of an area for home placement. ACS residential
// population; zero-population areas are excluded (weight 0).
[[nodiscard]] inline double
residentialWeight(const entity::geography::GeoArea &area) noexcept {
  return static_cast<double>(area.population);
}

class ResidenceSampler {
public:
  ResidenceSampler() = default;
  explicit ResidenceSampler(const entity::geography::GeoCatalog &catalog);

  // A population-weighted home area for `country`, or invalidGeoArea if
  // the catalogue has no residential area for it.
  [[nodiscard]] entity::geography::GeoAreaId
  sample(random::Rng &rng, locale::Country country) const;

  [[nodiscard]] bool has(locale::Country country) const noexcept;

private:
  struct CountryPool {
    std::vector<entity::geography::GeoAreaId> ids; // residential areas only
    std::vector<double> cdf;                       // parallel to ids
  };

  // Keyed by locale::Country's index.
  std::unordered_map<std::uint8_t, CountryPool> byCountry_;
};

} // namespace PhantomLedger::synth::geo
