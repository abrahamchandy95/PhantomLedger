#include "phantomledger/synth/geo/catalog.hpp"

#include "phantomledger/synth/geo/geo_data.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace PhantomLedger::synth::geo {

namespace {

using entity::geography::GeoArea;
using entity::geography::GeoAreaId;
using entity::geography::GeoCatalog;

// Builds the catalogue from the embedded rows (geo_data.hpp). Row
// order defines the 1-based GeoAreaId, exactly as the retired CSV's
// row order did — the embed round's contract is byte-neutrality:
// identical field values, landAreaKm2 left at its default 0 (the
// retired loader never populated it; a recorded code/data gap that
// must not silently change here).
[[nodiscard]] GeoCatalog build() {
  std::vector<GeoArea> areas;
  areas.reserve(data::kAreas.size());
  for (const auto &row : data::kAreas) {
    GeoArea a;
    a.id = static_cast<GeoAreaId>(areas.size() + 1);
    a.country = row.country;
    a.postalAreaCode = std::string{row.postalAreaCode};
    a.city = std::string{row.city};
    a.stateCode = std::string{row.stateCode};
    a.stateName = std::string{row.stateName};
    a.latitudeE6 = row.latitudeE6;
    a.longitudeE6 = row.longitudeE6;
    a.population = row.population;
    areas.push_back(std::move(a));
  }
  return GeoCatalog{std::move(areas)};
}

} // namespace

const GeoCatalog &geography() {
  static const GeoCatalog kCatalog = build();
  return kCatalog;
}

} // namespace PhantomLedger::synth::geo
