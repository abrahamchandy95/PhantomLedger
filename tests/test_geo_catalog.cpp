//
// tests/test_geo_catalog.cpp
//
// geo-causal-v1: the EMBEDDED world geography (synth::geo::geography()).
// No external file, no CLI — the catalogue is compiled in. Pins:
//   * coherent rows (city/state/postal/coordinates from one real place);
//   * 1-based dense ids; bounds-checked lookup;
//   * every US state + DC is represented (home placement covers the map);
//   * international destinations exist (travel / cross-border fraud);
//   * a real great-circle distance between two known cities (in MILES).
//

#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/synth/geo/catalog.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"

#include <cstdio>
#include <set>
#include <string>

namespace geo = ::PhantomLedger::entity::geography;
namespace synthgeo = ::PhantomLedger::synth::geo;
namespace locale = ::PhantomLedger::locale;

namespace {

int g_failures = 0;

void check(bool cond, const std::string &what) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

} // namespace

int main() {
  const auto &cat = synthgeo::geography();

  check(cat.size() >= 80, "embedded catalogue has >= 80 areas, got " +
                              std::to_string(cat.size()));

  // 1-based dense ids; row order defines them.
  bool idsDense = true;
  for (std::size_t i = 0; i < cat.areas().size(); ++i) {
    if (cat.areas()[i].id != static_cast<geo::GeoAreaId>(i + 1)) {
      idsDense = false;
    }
  }
  check(idsDense, "GeoArea ids are 1-based and dense");

  // Bounds.
  check(!cat.contains(geo::invalidGeoArea), "contains(invalid) is false");
  check(cat.contains(1), "contains(1) is true");
  check(cat.contains(static_cast<geo::GeoAreaId>(cat.size())),
        "contains(size) is true");
  check(!cat.contains(static_cast<geo::GeoAreaId>(cat.size() + 1)),
        "contains(size+1) is false");

  // Every US state + DC is represented, and each US row is coherent.
  std::set<std::string> usStates;
  const geo::GeoArea *ny = nullptr;
  const geo::GeoArea *la = nullptr;
  std::size_t intlCount = 0;
  for (const auto &a : cat.areas()) {
    if (a.country == locale::Country::us) {
      check(!a.city.empty() && !a.stateCode.empty() && !a.postalAreaCode.empty(),
            "US area has city/state/postal");
      check(a.longitudeE6 < 0, "US longitude is western (negative), city " +
                                   a.city);
      check(a.population > 0, "US area has population, city " + a.city);
      usStates.insert(a.stateCode);
      if (a.city == "New York") {
        ny = &a;
      }
      if (a.city == "Los Angeles") {
        la = &a;
      }
    } else {
      ++intlCount;
    }
  }
  check(usStates.size() == 51,
        "all 50 states + DC represented, got " +
            std::to_string(usStates.size()));
  check(intlCount >= 10, "international destinations exist, got " +
                             std::to_string(intlCount));

  check(ny != nullptr && ny->stateCode == "NY" && ny->postalAreaCode == "10001",
        "New York row is coherent");
  check(la != nullptr && la->stateCode == "CA", "Los Angeles row is coherent");

  if (ny != nullptr && la != nullptr) {
    // Distances are in MILES (US convention). NYC->LA great-circle is
    // ~2.45k miles (≈3.9k km); this pins both the value and the unit.
    const double d = geo::distanceMiles(*ny, *la);
    check(d > 2350.0 && d < 2550.0,
          "NYC->LA distance ~2.45k miles, got " + std::to_string(d));
  }

  // International destination present (e.g. London, GB).
  bool haveLondon = false;
  for (const auto &a : cat.areas()) {
    if (a.city == "London" && a.country == locale::Country::gb) {
      haveLondon = true;
    }
  }
  check(haveLondon, "London (GB) is present as an international destination");

  if (g_failures != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("test_geo_catalog: all checks passed (%zu areas)\n", cat.size());
  return 0;
}
