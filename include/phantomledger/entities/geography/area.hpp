#pragma once
//
// phantomledger/entities/geography/area.hpp
//
// The canonical geographic catalogue — a neutral world-model resource
// that neither PII synthesis nor any exporter owns. Homes (person +
// household), merchant outlets, and the card-fraud graph's City/State/
// Zipcode vertices ALL reference the same rows here, so a person's home
// area, the merchant they transact with, and the geography the exporter
// reports are the SAME canonical data — never independently invented.
//
// This is the G0 foundation of the geo-causal-v1 model change (see
// docs/fraud_model_audit.md and the systemprompt active arc). It
// carries data only; sampling (population-weighted homes, demand-based
// merchant placement) and selection (distance decay) live in the synth
// and activity layers that consume it.
//
// SEMANTICS (honesty note): an area is a PostalArea / GeoArea, NOT a
// literal USPS ZIP delivery route. The pinned production artifact is
// built from Census ZCTA Gazetteer geometry + ACS population + GeoNames
// postal names (see data/geo/README.md); ZCTAs are generalized areas
// and not every ZIP has one. The TigerGraph vertex keeps the name
// Zipcode for schema compatibility; its value is `postalAreaCode`.
//
// Coordinates are stored as INTEGER MICRODEGREES (degrees x 1e6):
// compact, stable to hash, deterministic to compare, and accurate to
// ~0.1 m — far finer than any distance decay needs. Never introduce
// double coordinates into the world model.
//

#include "phantomledger/taxonomies/locale/types.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace PhantomLedger::entity::geography {

// 1-based id; 0 is the "no area" sentinel so a default-constructed
// reference is invalid (an unassigned merchant/home never resolves to
// area row 0 by accident).
using GeoAreaId = std::uint32_t;
inline constexpr GeoAreaId invalidGeoArea = 0;

[[nodiscard]] constexpr bool validArea(GeoAreaId id) noexcept {
  return id != invalidGeoArea;
}

struct GeoArea {
  GeoAreaId id = invalidGeoArea;
  locale::Country country = locale::kDefaultCountry;

  std::string postalAreaCode; // ZCTA / postal-area code (NOT a ZIP route)
  std::string city;
  std::string stateCode; // "NY"
  std::string stateName; // "New York"

  std::int32_t latitudeE6 = 0;  // degrees x 1e6
  std::int32_t longitudeE6 = 0; // degrees x 1e6

  std::uint32_t population = 0;   // ACS residential population
  std::uint32_t landAreaKm2 = 0;  // Census land area (km^2)
};

// Great-circle distance in kilometres between two microdegree points.
// Pure and deterministic — the distance-decay selection score (G2) and
// the acceptance-gate distance reports build on this.
[[nodiscard]] inline double haversineKm(std::int32_t latE6A,
                                        std::int32_t lonE6A,
                                        std::int32_t latE6B,
                                        std::int32_t lonE6B) noexcept {
  constexpr double kE6 = 1'000'000.0;
  constexpr double kPi = 3.14159265358979323846;
  constexpr double kDegToRad = kPi / 180.0;
  constexpr double kEarthRadiusKm = 6371.0088;

  const double lat1 = (static_cast<double>(latE6A) / kE6) * kDegToRad;
  const double lat2 = (static_cast<double>(latE6B) / kE6) * kDegToRad;
  const double dLat = lat2 - lat1;
  const double dLon =
      ((static_cast<double>(lonE6B) - static_cast<double>(lonE6A)) / kE6) *
      kDegToRad;

  const double sinLat = std::sin(dLat * 0.5);
  const double sinLon = std::sin(dLon * 0.5);
  const double a =
      sinLat * sinLat + std::cos(lat1) * std::cos(lat2) * sinLon * sinLon;
  return 2.0 * kEarthRadiusKm * std::asin(std::sqrt(std::min(1.0, a)));
}

[[nodiscard]] inline double distanceKm(const GeoArea &a,
                                       const GeoArea &b) noexcept {
  return haversineKm(a.latitudeE6, a.longitudeE6, b.latitudeE6, b.longitudeE6);
}

// The loaded catalogue. Row order defines the 1-based ids; `at(id)`
// resolves in O(1). The container is immutable after load — a run
// pins one catalogue (one manifest vintage) for its whole lifetime.
class GeoCatalog {
public:
  GeoCatalog() = default;
  explicit GeoCatalog(std::vector<GeoArea> areas) noexcept
      : areas_(std::move(areas)) {}

  [[nodiscard]] std::size_t size() const noexcept { return areas_.size(); }
  [[nodiscard]] bool empty() const noexcept { return areas_.empty(); }

  [[nodiscard]] bool contains(GeoAreaId id) const noexcept {
    return validArea(id) && id <= areas_.size();
  }

  [[nodiscard]] const GeoArea &at(GeoAreaId id) const noexcept {
    // id is 1-based; row (id-1). Precondition: contains(id).
    return areas_[static_cast<std::size_t>(id) - 1];
  }

  [[nodiscard]] const std::vector<GeoArea> &areas() const noexcept {
    return areas_;
  }

private:
  std::vector<GeoArea> areas_;
};

} // namespace PhantomLedger::entity::geography
