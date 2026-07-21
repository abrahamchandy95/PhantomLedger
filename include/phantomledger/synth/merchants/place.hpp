#pragma once
//
// phantomledger/synth/merchants/place.hpp
//
// Merchant OUTLET geography (geo-causal-v1, G1c). After the economic
// catalogue is built (makeCatalog, from the shared entity stream), each
// merchant is assigned a Footprint and — for every physical footprint —
// a population-weighted US GeoArea, on ISOLATED per-merchant RNG lanes
// ({"merchant-footprint",serial} / {"merchant-geo",serial}) so placement
// never perturbs the shared stream. Online outlets are geography-free
// (location == invalidGeoArea). Catalogue merchants are DOMESTIC (US);
// international commerce appears only as travel / cross-border fraud
// EVENTS in G2.
//
// The assignment is consumed by the card-fraud merchant exporter, which
// resolves Record.location through synth::geo::geography() (the acausal
// derive::geoIndexFor/populationFor are deleted). G1c itself moved no
// golden — placement rides isolated lanes off the shared stream, so the
// corpus was byte-identical the round it landed. Placement is
// population-weighted only; category demand-density and urbanicity
// refinements await land-area / commercial data (owner's calibration
// pass) and are deliberately NOT fabricated here.
//

#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/primitives/random/distributions/cdf.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/synth/geo/catalog.hpp"
#include "phantomledger/synth/merchants/footprint.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace PhantomLedger::synth::merchants {

namespace geo = ::PhantomLedger::synth::geo;
namespace geoent = ::PhantomLedger::entity::geography;
namespace dist = ::PhantomLedger::probability::distributions;

namespace detail {

// The domestic (US) population-weighted area CDF, built once per placement
// pass. Zero-population areas carry no merchant (weight 0); international
// rows are excluded — catalogue merchants are domestic.
struct DomesticAreas {
  std::vector<geoent::GeoAreaId> ids;
  std::vector<double> cdf;
};

[[nodiscard]] inline DomesticAreas domesticAreas() {
  DomesticAreas out;
  std::vector<double> weights;
  for (const auto &area : geo::geography().areas()) {
    if (area.country != locale::Country::us || area.population == 0) {
      continue;
    }
    out.ids.push_back(area.id);
    weights.push_back(static_cast<double>(area.population));
  }
  if (!weights.empty()) {
    out.cdf = dist::buildCdf(weights);
  }
  return out;
}

[[nodiscard]] inline std::string_view serialView(std::array<char, 20> &buf,
                                                 std::uint64_t serial) {
  const auto [ptr, ec] =
      std::to_chars(buf.data(), buf.data() + buf.size(),
                    static_cast<unsigned long long>(serial));
  (void)ec;
  return std::string_view(buf.data(),
                          static_cast<std::size_t>(ptr - buf.data()));
}

} // namespace detail

// Assign Footprint + outlet location to every merchant on isolated lanes.
// Corpus-neutral: draws only on RngFactory{geoSeed}, never the shared rng.
inline void placeGeography(entity::merchant::Catalog &catalog,
                           std::uint64_t geoSeed) {
  const random::RngFactory factory{geoSeed};
  const auto areas = detail::domesticAreas();

  for (auto &rec : catalog.records) {
    std::array<char, 20> buf{};
    const auto serial = detail::serialView(buf, rec.label.value);

    auto footprintRng = factory.rng({"merchant-footprint", serial});
    rec.footprint = footprintFor(rec.category, footprintRng);

    if (rec.footprint == entity::merchant::Footprint::online ||
        areas.ids.empty()) {
      rec.location = geoent::invalidGeoArea;
      continue;
    }

    auto geoRng = factory.rng({"merchant-geo", serial});
    const auto idx = dist::sampleIndex(areas.cdf, geoRng.nextDouble());
    rec.location = areas.ids[idx];
  }
}

} // namespace PhantomLedger::synth::merchants
