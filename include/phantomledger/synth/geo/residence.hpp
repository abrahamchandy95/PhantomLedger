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
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
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

  // relocation-2026-07: the same population weighting, RESTRICTED to one
  // state. CPS composition puts 81.8% of domestic moves inside the origin's
  // state, so a mover needs an in-state draw.
  //
  // TAKES A PRE-DRAWN UNIFORM rather than the Rng, and that is deliberate:
  // the caller must spend the SAME number of uniforms whether the in-state
  // pool exists or not, so a catalogue that gains or loses a state's
  // residential areas cannot shift the schedule of unrelated households.
  // Falls back to the country-wide pool — with the same `u` — when the state
  // has no residential area, which is the case for every state the pinned
  // 71-city catalogue does not reach.
  [[nodiscard]] entity::geography::GeoAreaId
  sampleInState(double u, locale::Country country,
                std::string_view stateCode) const;

  [[nodiscard]] entity::geography::GeoAreaId
  sampleWith(double u, locale::Country country) const;

  // relocation-2026-07: a population-weighted destination that is NOT
  // `exclude`, restricted to `stateCode` when that state has two or more
  // residential areas and country-wide otherwise.
  //
  // THE EXCLUSION IS THE POINT, and it was a MEASURED correction rather than a
  // refinement. Sampling the full pool and dropping a self-hit as a no-op
  // delivered HALF the intended move rate — 0.0511 moves/person-year against a
  // CPS-derived 0.103 — because the pinned catalogue carries only a handful of
  // areas per state, so an in-state redraw lands on the origin often. It also
  // biased the realized same-state share DOWN to 0.63 from a nominal 0.818,
  // since small in-state pools self-hit more than the national pool does.
  // Conditional on moving, a household moves SOMEWHERE ELSE; renormalising
  // over the origin's complement is what makes the realized rate the rate the
  // series states.
  //
  // Returns `invalidGeoArea` only when no alternative exists at all.
  [[nodiscard]] entity::geography::GeoAreaId
  sampleExcluding(double u, locale::Country country, std::string_view stateCode,
                  entity::geography::GeoAreaId exclude) const;

private:
  struct CountryPool {
    std::vector<entity::geography::GeoAreaId> ids; // residential areas only
    std::vector<double> weights;                   // parallel to ids
    std::vector<double> cdf;                       // parallel to ids
  };

  [[nodiscard]] static entity::geography::GeoAreaId
  pickExcluding(const CountryPool &pool, double u,
                entity::geography::GeoAreaId exclude) noexcept;

  // Keyed by locale::Country's index.
  std::unordered_map<std::uint8_t, CountryPool> byCountry_;
  // Keyed by country index + stateCode. Same rows, partitioned by state.
  std::map<std::pair<std::uint8_t, std::string>, CountryPool> byState_;
};

} // namespace PhantomLedger::synth::geo
