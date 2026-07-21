#pragma once
//
// phantomledger/activity/spending/market/commerce/geo_pools.hpp
//
// geo-causal-v1 (G2a step-2): per-home-area distance-decay pools over the
// PHYSICAL merchants in the catalogue. Card-present everyday spend is LOCAL —
// a customer is far likelier to pay at an outlet near home — so for each home
// area we precompute a CDF over the physical outlets, weighted by
//
//     popularity(record.weight) * exp(-distanceMiles / scaleMiles(homeArea))
//
// Distance is in MILES (US retail-banking convention; see
// entities/geography/area.hpp). Online / non-physical merchants
// (location == invalidGeoArea) are NOT in any pool — they are reached through
// the geography-free national CDF (the online branch of merchant selection).
//
// LOCATION-VARYING DECAY (owner 2026-07-21): the decay scale is a FUNCTION OF
// THE CUSTOMER'S HOME AREA, not a global constant. A New Yorker's everyday
// card-present radius is a few miles (merchants are dense, nobody drives far);
// a small-town / rural resident routinely travels tens of miles. Urbanicity is
// proxied here by the area's POPULATION — the only signal in the catalogue
// today (no land area yet) — log-interpolated between a dense-urban and a
// small-town scale. PROVISIONAL: the endpoints, the curve, and the switch to
// true density (population / land_area) are calibration debt (see systemprompt
// DISTANCE UNITS + DECAY SCALE).
//
// Popularity stays LOCATION-INDEPENDENT: the same record.weight feeds both the
// national CDF and every area pool, so a big-city merchant earns higher
// realized volume purely by having more nearby customers, never by a wired
// coupling (axiom 7). The structure is PURE data built from the catalogue +
// geography — no RNG is consumed here, so building it moves no golden.
//
// Memory is O(occupiedAreas x physicalMerchants); fine at the placeholder
// catalogue (~71 areas), a scaling item folded into the catalogue-expansion
// debt.
//

#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/primitives/random/distributions/cdf.hpp"

#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::activity::spending::market::commerce {

// The card-present distance-decay scale (MILES) for a customer whose home is
// `homeArea`. Denser/larger places decay FAST; smaller/rural places decay
// SLOWLY. PROVISIONAL population-proxy curve (see the file header + debt).
[[nodiscard]] inline double
cardPresentDecayScaleMiles(const entity::geography::GeoArea &homeArea) noexcept {
  constexpr double kUrbanScaleMiles = 4.0;      // mega-city everyday radius
  constexpr double kSmallTownScaleMiles = 50.0; // small/rural everyday radius
  constexpr double kPopUrban = 5'000'000.0;     // >= this -> kUrbanScaleMiles
  constexpr double kPopSmallTown = 50'000.0;    // <= this -> kSmallTownScaleMiles

  const double p = static_cast<double>(homeArea.population);
  if (!(p > kPopSmallTown)) {
    return kSmallTownScaleMiles;
  }
  if (p >= kPopUrban) {
    return kUrbanScaleMiles;
  }

  // Log-linear between the two anchors: larger population -> shorter scale.
  const double t = (std::log(p) - std::log(kPopSmallTown)) /
                   (std::log(kPopUrban) - std::log(kPopSmallTown));
  return kSmallTownScaleMiles + t * (kUrbanScaleMiles - kSmallTownScaleMiles);
}

class GeographicMerchantPools {
public:
  GeographicMerchantPools() = default;

  // Build a distance-decay pool for each DISTINCT home area in `homeAreas`,
  // using a per-home-area decay scale (cardPresentDecayScaleMiles). Returns an
  // empty (has()==false everywhere) instance when there is nothing to build —
  // no catalogue, no geography, no home areas, or no physical merchants — so
  // callers fall back to the national CDF safely.
  [[nodiscard]] static GeographicMerchantPools
  build(const entity::merchant::Catalog &catalog,
        const entity::geography::GeoCatalog &geo,
        std::span<const entity::geography::GeoAreaId> homeAreas) {
    GeographicMerchantPools pools;

    if (catalog.records.empty() || geo.empty() || homeAreas.empty()) {
      return pools;
    }

    // Physical (card-present-reachable) merchants, in catalog order. Every
    // area CDF is aligned to this list: cdf[k] corresponds to physical_[k].
    for (std::uint32_t i = 0; i < catalog.records.size(); ++i) {
      const auto area = catalog.records[i].location;
      if (entity::geography::validArea(area) && geo.contains(area)) {
        pools.physical_.push_back(i);
      }
    }
    if (pools.physical_.empty()) {
      return pools;
    }

    pools.areaToPool_.assign(geo.size() + 1, -1);

    for (const auto home : homeAreas) {
      if (!entity::geography::validArea(home) || !geo.contains(home) ||
          pools.areaToPool_[home] >= 0) {
        continue; // out of range, or already built for this area
      }

      auto cdf = pools.buildAreaCdf(catalog, geo, home);
      if (cdf.empty()) {
        continue; // degenerate (all-zero) weights — leave to national CDF
      }

      pools.areaToPool_[home] =
          static_cast<std::int32_t>(pools.cdfs_.size());
      pools.cdfs_.push_back(std::move(cdf));
    }

    return pools;
  }

  [[nodiscard]] bool empty() const noexcept { return physical_.empty(); }

  // True iff a distance-decay pool exists for `homeArea`.
  [[nodiscard]] bool
  has(entity::geography::GeoAreaId homeArea) const noexcept {
    return homeArea < areaToPool_.size() && areaToPool_[homeArea] >= 0;
  }

  // Sample a CATALOG record index from the home area's distance-decay pool,
  // given u in [0,1). Precondition: has(homeArea).
  [[nodiscard]] std::uint32_t
  sample(entity::geography::GeoAreaId homeArea, double u) const {
    const auto pool = static_cast<std::size_t>(areaToPool_[homeArea]);
    const auto k = probability::distributions::sampleIndex(cdfs_[pool], u);
    return physical_[k];
  }

private:
  // Build the CDF for one home area over physical_. Returns an empty vector if
  // the summed weight is not finite-positive (so no pool is registered).
  [[nodiscard]] std::vector<double>
  buildAreaCdf(const entity::merchant::Catalog &catalog,
               const entity::geography::GeoCatalog &geo,
               entity::geography::GeoAreaId home) const {
    const auto &homeArea = geo.at(home);
    const double scaleMiles = cardPresentDecayScaleMiles(homeArea);

    std::vector<double> weights;
    weights.reserve(physical_.size());

    double total = 0.0;
    for (const auto idx : physical_) {
      const auto &rec = catalog.records[idx];
      const double miles =
          entity::geography::distanceMiles(homeArea, geo.at(rec.location));
      const double w = rec.weight * std::exp(-miles / scaleMiles);
      weights.push_back(w);
      total += w;
    }

    if (!(total > 0.0) || !std::isfinite(total)) {
      return {};
    }

    return probability::distributions::buildCdf(weights);
  }

  // Catalog record indices of physical merchants (validArea location), in
  // catalog order; shared by every area CDF.
  std::vector<std::uint32_t> physical_;

  // areaToPool_[areaId] = index into cdfs_, or -1 if no pool. Dense over
  // [0, geo.size()]; only occupied areas allocate a CDF.
  std::vector<std::int32_t> areaToPool_;

  // One CDF per occupied area, each aligned to and sized like physical_.
  std::vector<std::vector<double>> cdfs_;
};

} // namespace PhantomLedger::activity::spending::market::commerce
