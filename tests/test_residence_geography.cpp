//
// tests/test_residence_geography.cpp
//
// geo-causal-v1: population-weighted home placement over the catalogue
// (synth::geo::ResidenceSampler). Pins that
//   * homes are weighted by residential population (dense areas chosen
//     far more than sparse ones) — NOT uniform, the bug this replaces;
//   * zero-residential areas (PO-box / group-quarters) are NEVER homes;
//   * placement is per-country (a country with no residential area
//     yields invalidGeoArea);
//   * sampling is deterministic for a fixed RNG seed;
//   * over the EMBEDDED world catalogue, US homes are valid US areas.
//
// In-test catalogues drive the edge invariants; the embedded
// geography() drives the end-to-end smoke. No external fixture.
//

#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/synth/geo/catalog.hpp"
#include "phantomledger/synth/geo/residence.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"

#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace geo = ::PhantomLedger::entity::geography;
namespace synthgeo = ::PhantomLedger::synth::geo;
namespace locale = ::PhantomLedger::locale;
using ::PhantomLedger::random::Rng;

namespace {

int g_failures = 0;

void check(bool cond, const std::string &what) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

geo::GeoArea area(geo::GeoAreaId id, locale::Country country,
                  std::uint32_t population) {
  geo::GeoArea a;
  a.id = id;
  a.country = country;
  a.population = population;
  a.stateCode = "XX";
  a.city = "City" + std::to_string(id);
  return a;
}

} // namespace

int main() {
  // ---- In-test catalogue: weighting, zero-pop exclusion, country ----
  // US areas: dense(1000), sparse(10), zero(0, excluded). One GB area.
  std::vector<geo::GeoArea> rows{
      area(1, locale::Country::us, 1000),
      area(2, locale::Country::us, 10),
      area(3, locale::Country::us, 0),
      area(4, locale::Country::gb, 500),
  };
  const geo::GeoCatalog cat{std::move(rows)};
  const synthgeo::ResidenceSampler sampler{cat};

  check(sampler.has(locale::Country::us), "sampler has US");
  check(sampler.has(locale::Country::gb), "sampler has GB");
  check(!sampler.has(locale::Country::fr),
        "sampler does NOT have FR (no areas)");

  constexpr int kDraws = 40'000;
  std::unordered_map<geo::GeoAreaId, int> usCounts;
  {
    auto rng = Rng::fromSeed(0xC0FFEEULL);
    for (int i = 0; i < kDraws; ++i) {
      ++usCounts[sampler.sample(rng, locale::Country::us)];
    }
  }
  check(usCounts[3] == 0, "zero-population area is never a home");
  check(usCounts.find(geo::invalidGeoArea) == usCounts.end(),
        "US sampling never yields invalidGeoArea (US has residential areas)");
  check(usCounts[1] > usCounts[2],
        "denser area (pop 1000) chosen more than sparser (pop 10)");
  check(usCounts[1] > 10 * usCounts[2],
        "population weighting is ~proportional (dense >> sparse), got " +
            std::to_string(usCounts[1]) + " vs " +
            std::to_string(usCounts[2]));

  // GB has a single residential area -> always area 4.
  {
    auto rng = Rng::fromSeed(7ULL);
    bool allGb = true;
    for (int i = 0; i < 1000; ++i) {
      if (sampler.sample(rng, locale::Country::gb) != 4) {
        allGb = false;
      }
    }
    check(allGb, "GB always resolves to its one residential area");
  }

  // A country with no areas -> invalidGeoArea.
  {
    auto rng = Rng::fromSeed(7ULL);
    check(sampler.sample(rng, locale::Country::fr) == geo::invalidGeoArea,
          "country with no residential area yields invalidGeoArea");
  }

  // Determinism: same seed -> same sequence.
  {
    auto a = Rng::fromSeed(99ULL);
    auto b = Rng::fromSeed(99ULL);
    bool identical = true;
    for (int i = 0; i < 5000; ++i) {
      if (sampler.sample(a, locale::Country::us) !=
          sampler.sample(b, locale::Country::us)) {
        identical = false;
      }
    }
    check(identical, "same seed yields the same home sequence");
  }

  // ---- Embedded world catalogue: valid, in-country US placement ----
  {
    const auto &world = synthgeo::geography();
    const synthgeo::ResidenceSampler worldSampler{world};
    check(worldSampler.has(locale::Country::us), "embedded catalogue has US");

    auto rng = Rng::fromSeed(12345ULL);
    bool allValidUs = true;
    for (int i = 0; i < 5000; ++i) {
      const auto id = worldSampler.sample(rng, locale::Country::us);
      if (!world.contains(id) ||
          world.at(id).country != locale::Country::us) {
        allValidUs = false;
      }
    }
    check(allValidUs,
          "every embedded home is a valid US area from the same catalogue");
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("test_residence_geography: all checks passed\n");
  return 0;
}
